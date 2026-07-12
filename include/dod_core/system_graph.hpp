#pragma once

#include <cstddef>
#include <dod_core/system.hpp>
#include <utility>
#include <vector>

namespace dod
{

using NodeId = std::size_t;

class SystemGraph
{
  public:
    SystemGraph() = default;

    SystemGraph(const SystemGraph&) = delete;
    SystemGraph& operator=(const SystemGraph&) = delete;

    // Custom move ops: defaulted move would memberwise-copy the m_built bool,
    // leaving the moved-from graph reporting built()==true with an empty node
    // set. We reset m_built to false on the source so moved-from graphs report
    // their actual (unbuilt, empty) state.
    SystemGraph(SystemGraph&& other) noexcept;
    SystemGraph& operator=(SystemGraph&& other) noexcept;

    // Add a system to the graph. Returns its NodeId. Must be called before build().
    NodeId add_system(System system);

    // Declare an explicit ordering: `before` must execute before `after`.
    // Both must be valid NodeIds. Must be called before build().
    void order_before(NodeId before, NodeId after);

    // Analyze conflicts, materialize the adjacency lists, and verify acyclicity.
    // Returns true on success (and on repeat calls once built). Returns false
    // if the edges form a cycle; the graph then remains unbuilt and must be
    // discarded after fixing the offending order_before() declarations.
    [[nodiscard]] bool build();

    [[nodiscard]] std::size_t size() const noexcept { return m_nodes.size(); }
    [[nodiscard]] bool built() const noexcept { return m_built; }

    [[nodiscard]] const System& system(NodeId id) const;
    [[nodiscard]] const std::vector<NodeId>& roots() const noexcept { return m_roots; }
    [[nodiscard]] const std::vector<NodeId>& dependents(NodeId id) const;
    [[nodiscard]] const std::vector<NodeId>& dependencies(NodeId id) const;
    [[nodiscard]] std::size_t dependency_count(NodeId id) const;

    // A valid dependency-respecting execution order, cached at build(). Empty
    // until built(). Iterating it and running each system in turn is a correct
    // single-threaded execution (used by the zero-worker dispatch path); the
    // parallel scheduler uses the adjacency lists directly instead.
    [[nodiscard]] const std::vector<NodeId>& topological_order() const noexcept
    {
        return m_topological_order;
    }

  private:
    struct Node
    {
        System system;
        std::vector<NodeId> dependents;
        std::vector<NodeId> dependencies;
    };

    static bool conflicts(const ResourceAccess& a, const ResourceAccess& b) noexcept;
    void add_edge(NodeId from, NodeId to);
    // Kahn's algorithm: fills m_topological_order and returns true iff the
    // graph is acyclic (order then covers every node).
    [[nodiscard]] bool compute_topological_order();
    void check_id(NodeId id) const;

    std::vector<Node> m_nodes;
    std::vector<std::pair<NodeId, NodeId>> m_explicit_edges;
    std::vector<NodeId> m_roots;
    std::vector<NodeId> m_topological_order;
    bool m_built = false;
};

} // namespace dod
