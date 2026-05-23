#pragma once

#include <atomic>
#include <cstddef>
#include <dod_core/system_graph.hpp>
#include <dod_core/thread_pool.hpp>
#include <vector>

namespace dod
{

namespace detail
{

// Runs every system in `graph` exactly once through `pool`, respecting edges.
// `counters` must already be sized to graph.size(); it is rewritten on entry.
// Blocks until all in-flight tasks finish. If any system throws, rethrows the
// first exception after the wait completes.
void dispatch_graph(const SystemGraph& graph, World& world, ThreadPool& pool,
                    std::vector<std::atomic<std::size_t>>& counters);

} // namespace detail

class Scheduler
{
  public:
    explicit Scheduler(SystemGraph graph, std::size_t worker_count = default_worker_count());

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    // Execute every system once, respecting graph ordering. Blocks until all
    // systems finish. If any system throws, run() rethrows the first exception
    // after all in-flight tasks have completed.
    void run(World& world);

    [[nodiscard]] const SystemGraph& graph() const noexcept { return m_graph; }
    [[nodiscard]] std::size_t worker_count() const noexcept { return m_pool.worker_count(); }

  private:
    static std::size_t default_worker_count() noexcept;

    SystemGraph m_graph;
    ThreadPool m_pool;
    std::vector<std::atomic<std::size_t>> m_remaining;
};

} // namespace dod
