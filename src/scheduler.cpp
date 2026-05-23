#include <dod_core/scheduler.hpp>
#include <exception>
#include <functional>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace dod
{

Scheduler::Scheduler(SystemGraph graph, std::size_t worker_count)
    : m_graph{std::move(graph)}, m_pool{worker_count}, m_remaining(m_graph.size())
{
    if (!m_graph.built())
    {
        throw std::logic_error("Scheduler: SystemGraph must be built() before construction");
    }
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

    // Reset per-node counters from the graph's static dependency counts.
    for (NodeId i = 0; i < n; ++i)
    {
        counters[i].store(graph.dependency_count(i), std::memory_order_relaxed);
    }

    std::latch done{static_cast<std::ptrdiff_t>(n)};
    std::exception_ptr first_exception;
    std::mutex exc_mutex;

    // Recursive dispatch lambda. Captured by reference; lifetime extends until
    // this function returns, which only happens after the latch fully resolves.
    std::function<void(NodeId)> dispatch;
    dispatch = [&](NodeId id)
    {
        try
        {
            graph.system(id)(world);
        }
        catch (...)
        {
            std::lock_guard lock(exc_mutex);
            if (!first_exception)
            {
                first_exception = std::current_exception();
            }
        }

        // Advance the graph regardless of failure to avoid deadlocking the
        // latch on a partially-completed run.
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

    if (first_exception)
    {
        std::rethrow_exception(first_exception);
    }
}

} // namespace detail

} // namespace dod
