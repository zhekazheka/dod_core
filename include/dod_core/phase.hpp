#pragma once

#include <atomic>
#include <cstddef>
#include <dod_core/scheduler.hpp>
#include <dod_core/system_graph.hpp>
#include <dod_core/thread_pool.hpp>
#include <dod_core/world.hpp>
#include <string>
#include <vector>

namespace dod
{

class Phase
{
  public:
    Phase(std::string name, SystemGraph graph);

    Phase(const Phase&) = delete;
    Phase& operator=(const Phase&) = delete;
    Phase(Phase&&) noexcept = default;
    Phase& operator=(Phase&&) noexcept = default;

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] const SystemGraph& graph() const noexcept { return m_graph; }

  private:
    std::string m_name;
    SystemGraph m_graph;
};

class Schedule
{
  public:
    explicit Schedule(std::size_t worker_count = default_worker_count());

    Schedule(const Schedule&) = delete;
    Schedule& operator=(const Schedule&) = delete;
    Schedule(Schedule&&) = delete;
    Schedule& operator=(Schedule&&) = delete;

    // Phases execute in the order they are added.
    void add_phase(Phase phase);

    // Run every phase sequentially. Within a phase, systems run in parallel
    // through the shared thread pool. If a system in any phase throws, run()
    // rethrows after that phase's wait; subsequent phases are skipped.
    void run(World& world);

    [[nodiscard]] std::size_t phase_count() const noexcept { return m_phases.size(); }
    [[nodiscard]] const Phase& phase(std::size_t index) const;
    [[nodiscard]] std::size_t worker_count() const noexcept { return m_pool.worker_count(); }

  private:
    static std::size_t default_worker_count() noexcept;

    struct Entry
    {
        Phase phase;
        std::vector<std::atomic<std::size_t>> counters;

        explicit Entry(Phase p);

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;
        Entry(Entry&&) noexcept = default;
        Entry& operator=(Entry&&) noexcept = default;
    };

    ThreadPool m_pool;
    std::vector<Entry> m_phases;
};

} // namespace dod
