#include <dod_core/assert.hpp>
#include <dod_core/scheduler.hpp>

#include <latch>
#include <thread>

namespace dod
{

Scheduler::Scheduler(SystemGraph graph, std::size_t worker_count)
    : m_graph{std::move(graph)}, m_pool{worker_count}, m_remaining(m_graph.size())
{
    DOD_ASSERT(m_graph.built(), "Scheduler: SystemGraph must be built() before construction");
}

std::size_t Scheduler::default_worker_count() noexcept
{
    // The calling thread participates in execution during run(), so spawn one
    // worker fewer than the core count to keep total active threads at hw.
    const auto hw = std::thread::hardware_concurrency();
    return hw <= 1 ? 1u : static_cast<std::size_t>(hw - 1);
}

void Scheduler::run(World& world)
{
    detail::dispatch_graph(m_graph, world, m_pool, m_remaining);
}

namespace detail
{

namespace
{

constexpr NodeId invalid_node = static_cast<NodeId>(-1);

// Per-run dispatch state. Lives on the dispatching thread's stack; workers
// hold only a pointer to it. dispatch_graph returns after the latch fully
// resolves, and count_down is each node's final touch of this object, so no
// worker can observe it after destruction.
struct Dispatch
{
    const SystemGraph& graph;
    World& world;
    ThreadPool& pool;
    std::vector<std::atomic<std::size_t>>& counters;
    std::latch& done;

    // Run `id`, then keep running newly-ready dependents inline: the first
    // ready dependent continues on this thread, the rest go to the pool. A
    // serial chain therefore executes with zero queue round trips after its
    // root. Systems must not throw — exceptions are disabled project-wide.
    void run_chain(NodeId id)
    {
        while (true)
        {
            graph.system(id)(world);

            NodeId next = invalid_node;
            for (NodeId dep : graph.dependents(id))
            {
                if (counters[dep].fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    if (next == invalid_node)
                    {
                        next = dep;
                    }
                    else
                    {
                        pool.submit_detached([this, dep] { run_chain(dep); });
                    }
                }
            }

            // May be the final count_down of the frame, after which the
            // dispatching thread can wake and destroy *this. Only the local
            // `next` may be read past this point unless next != invalid_node
            // (an unfinished node keeps the latch, and so *this, alive).
            done.count_down();

            if (next == invalid_node)
            {
                return;
            }
            id = next;
        }
    }
};

} // namespace

void dispatch_graph(const SystemGraph& graph, World& world, ThreadPool& pool,
                    std::vector<std::atomic<std::size_t>>& counters)
{
    // Refuse to run an unbuilt graph. Without built adjacency lists there are
    // no roots, so dispatching would block on the latch forever. Asserts in
    // debug; in release a no-op run is the recoverable failure mode.
    if (!graph.built())
    {
        DOD_ASSERT(false, "dispatch_graph: SystemGraph must be built()");
        return;
    }

    const std::size_t n = graph.size();
    if (n == 0)
    {
        return;
    }

    DOD_ASSERT(counters.size() >= n, "dispatch_graph: counters must be sized to graph.size()");

    // Pre-create every component pool the graph touches, on this thread.
    // EnTT creates pools lazily and that creation is not thread-safe, so it
    // must never first happen inside a worker running systems in parallel.
    // For already-existing pools this is one map lookup per query parameter.
    for (NodeId i = 0; i < n; ++i)
    {
        graph.system(i).prepare(world);
    }

    // Zero-worker pool: run entirely on the calling thread in the cached
    // topological order. No task queue, no latch, no std::function — this path
    // allocates nothing per run, which callers that need a single-threaded,
    // allocation-free tick (e.g. a networking library's per-frame update)
    // depend on. `counters` is unused here.
    if (pool.worker_count() == 0)
    {
        for (NodeId id : graph.topological_order())
        {
            graph.system(id)(world);
        }
        return;
    }

    // Reset per-node counters from the graph's static dependency counts.
    for (NodeId i = 0; i < n; ++i)
    {
        counters[i].store(graph.dependency_count(i), std::memory_order_relaxed);
    }

    std::latch done{static_cast<std::ptrdiff_t>(n)};
    Dispatch dispatch{graph, world, pool, counters, done};

    // Hand all roots but the first to the pool; this thread runs the first
    // root (and its inline continuation chain) itself instead of parking.
    const auto& roots = graph.roots();
    DOD_ASSERT(!roots.empty(), "dispatch_graph: built non-empty graph has no roots");
    for (std::size_t i = 1; i < roots.size(); ++i)
    {
        pool.submit_detached([&dispatch, id = roots[i]] { dispatch.run_chain(id); });
    }
    dispatch.run_chain(roots.front());

    // Keep helping with queued work while any is visible, then block for the
    // tail running on workers. (try_wait may spuriously return false; the
    // final done.wait() is what guarantees completion.)
    while (!done.try_wait() && pool.try_run_one())
    {
    }
    done.wait();
}

} // namespace detail

} // namespace dod
