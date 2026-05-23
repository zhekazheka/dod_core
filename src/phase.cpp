#include <dod_core/phase.hpp>
#include <stdexcept>
#include <thread>
#include <utility>

namespace dod
{

Phase::Phase(std::string name, SystemGraph graph)
    : m_name{std::move(name)}, m_graph{std::move(graph)}
{
    if (!m_graph.built())
    {
        throw std::logic_error("Phase: SystemGraph must be built() before construction");
    }
}

Schedule::Entry::Entry(Phase p) : phase{std::move(p)}, counters(this->phase.graph().size()) {}

Schedule::Schedule(std::size_t worker_count) : m_pool{worker_count} {}

void Schedule::add_phase(Phase phase)
{
    m_phases.emplace_back(std::move(phase));
}

void Schedule::run(World& world)
{
    for (auto& entry : m_phases)
    {
        detail::dispatch_graph(entry.phase.graph(), world, m_pool, entry.counters);
    }
}

const Phase& Schedule::phase(std::size_t index) const
{
    if (index >= m_phases.size())
    {
        throw std::out_of_range("Schedule::phase: invalid index");
    }
    return m_phases[index].phase;
}

std::size_t Schedule::default_worker_count() noexcept
{
    const auto hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1u : static_cast<std::size_t>(hw);
}

} // namespace dod
