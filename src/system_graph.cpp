#include <algorithm>
#include <dod_core/assert.hpp>
#include <dod_core/system_graph.hpp>

namespace dod
{

namespace
{

bool contains(const std::vector<entt::id_type>& haystack, entt::id_type needle) noexcept
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

} // namespace

SystemGraph::SystemGraph(SystemGraph&& other) noexcept
    : m_nodes{std::move(other.m_nodes)}, m_explicit_edges{std::move(other.m_explicit_edges)},
      m_roots{std::move(other.m_roots)}, m_built{other.m_built}
{
    other.m_built = false;
}

SystemGraph& SystemGraph::operator=(SystemGraph&& other) noexcept
{
    if (this != &other)
    {
        m_nodes = std::move(other.m_nodes);
        m_explicit_edges = std::move(other.m_explicit_edges);
        m_roots = std::move(other.m_roots);
        m_built = other.m_built;
        other.m_built = false;
    }
    return *this;
}

NodeId SystemGraph::add_system(System system)
{
    DOD_ASSERT(!m_built, "SystemGraph::add_system called after build()");
    NodeId id = m_nodes.size();
    m_nodes.push_back(Node{std::move(system), {}, {}});
    return id;
}

void SystemGraph::order_before(NodeId before, NodeId after)
{
    DOD_ASSERT(!m_built, "SystemGraph::order_before called after build()");
    check_id(before);
    check_id(after);
    DOD_ASSERT(before != after, "SystemGraph::order_before: cannot order a node before itself");
    m_explicit_edges.emplace_back(before, after);
}

void SystemGraph::build()
{
    if (m_built)
    {
        return;
    }

    // User-declared edges first, so they take priority over registration-order
    // resolution for conflicting pairs. Two contradicting explicit edges land here
    // as both directions of an edge and are caught later by detect_cycles().
    for (const auto& [before, after] : m_explicit_edges)
    {
        add_edge(before, after);
    }

    // Conflict-derived edges: for each pair (i, j) with i < j, if they conflict,
    // i runs before j (registration order). Skip if the user already declared an
    // ordering between this pair in either direction.
    for (NodeId i = 0; i < m_nodes.size(); ++i)
    {
        for (NodeId j = i + 1; j < m_nodes.size(); ++j)
        {
            if (!conflicts(m_nodes[i].system.access(), m_nodes[j].system.access()))
            {
                continue;
            }
            const auto& deps_j = m_nodes[j].dependencies;
            const auto& deps_i = m_nodes[i].dependencies;
            const bool already_constrained =
                std::find(deps_j.begin(), deps_j.end(), i) != deps_j.end() ||
                std::find(deps_i.begin(), deps_i.end(), j) != deps_i.end();
            if (!already_constrained)
            {
                add_edge(i, j);
            }
        }
    }

    // Roots: nodes with no incoming edges.
    m_roots.clear();
    for (NodeId i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].dependencies.empty())
        {
            m_roots.push_back(i);
        }
    }

    detect_cycles();

    m_built = true;
}

const System& SystemGraph::system(NodeId id) const
{
    check_id(id);
    return m_nodes[id].system;
}

const std::vector<NodeId>& SystemGraph::dependents(NodeId id) const
{
    check_id(id);
    return m_nodes[id].dependents;
}

const std::vector<NodeId>& SystemGraph::dependencies(NodeId id) const
{
    check_id(id);
    return m_nodes[id].dependencies;
}

std::size_t SystemGraph::dependency_count(NodeId id) const
{
    check_id(id);
    return m_nodes[id].dependencies.size();
}

bool SystemGraph::conflicts(const ResourceAccess& a, const ResourceAccess& b) noexcept
{
    // WorldWrite is exclusive: blocks anything else.
    if (a.world_write || b.world_write)
    {
        return true;
    }

    // WorldRead conflicts with any component write (since it could observe it).
    if (a.world_read && !b.writes.empty())
    {
        return true;
    }
    if (b.world_read && !a.writes.empty())
    {
        return true;
    }

    // a writes something b reads or writes.
    for (auto w : a.writes)
    {
        if (contains(b.reads, w) || contains(b.writes, w))
        {
            return true;
        }
    }

    // b writes something a reads (the writes/writes case is already covered above).
    for (auto w : b.writes)
    {
        if (contains(a.reads, w))
        {
            return true;
        }
    }

    return false;
}

void SystemGraph::add_edge(NodeId from, NodeId to)
{
    auto& deps = m_nodes[to].dependencies;
    if (std::find(deps.begin(), deps.end(), from) != deps.end())
    {
        return; // already present
    }
    deps.push_back(from);
    m_nodes[from].dependents.push_back(to);
}

void SystemGraph::detect_cycles() const
{
    std::vector<std::size_t> incoming(m_nodes.size());
    for (NodeId i = 0; i < m_nodes.size(); ++i)
    {
        incoming[i] = m_nodes[i].dependencies.size();
    }

    std::vector<NodeId> queue;
    queue.reserve(m_roots.size());
    for (NodeId id : m_roots)
    {
        queue.push_back(id);
    }

    std::size_t processed = 0;
    while (!queue.empty())
    {
        NodeId id = queue.back();
        queue.pop_back();
        ++processed;
        for (NodeId d : m_nodes[id].dependents)
        {
            if (--incoming[d] == 0)
            {
                queue.push_back(d);
            }
        }
    }

    DOD_ASSERT(processed == m_nodes.size(), "SystemGraph contains a cycle");
}

void SystemGraph::check_id(NodeId id) const
{
    DOD_ASSERT(id < m_nodes.size(), "SystemGraph: invalid NodeId");
}

} // namespace dod
