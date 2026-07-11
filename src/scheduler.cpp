#include <dod_core/assert.hpp>
#include <dod_core/scheduler.hpp>

#include <functional>
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
    const auto hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1u : static_cast<std::size_t>(hw);
}

void Scheduler::run(World& world)
{
    detail::dispatch_graph(m_graph, world, m_pool, m_remaining);
}

namespace detail
{

void dispatch_graph(const SystemGraph& graph, World& world, ThreadPool& pool,
                    std::vector<std::atomic<std::size_t>>& counters)
{
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

    // Reset per-node counters from the graph's static dependency counts.
    for (NodeId i = 0; i < n; ++i)
    {
        counters[i].store(graph.dependency_count(i), std::memory_order_relaxed);
    }

    std::latch done{static_cast<std::ptrdiff_t>(n)};

    // Recursive dispatch lambda. Captured by reference; lifetime extends until
    // this function returns, which only happens after the latch fully resolves.
    // Systems must not throw — exceptions are disabled at the project level.
    std::function<void(NodeId)> dispatch;
    dispatch = [&](NodeId id)
    {
        graph.system(id)(world);

        for (NodeId dep : graph.dependents(id))
        {
            if (counters[dep].fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                pool.submit_detached([&dispatch, dep]() { dispatch(dep); });
            }
        }
        done.count_down();
    };

    for (NodeId root : graph.roots())
    {
        pool.submit_detached([&dispatch, root]() { dispatch(root); });
    }

    done.wait();
}

} // namespace detail

} // namespace dod
