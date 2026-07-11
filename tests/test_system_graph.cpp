#include <algorithm>
#include <dod_core/system_graph.hpp>
#include <gtest/gtest.h>

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

struct Health
{
    int hp;
};

namespace
{

bool contains(const std::vector<dod::NodeId>& v, dod::NodeId id)
{
    return std::find(v.begin(), v.end(), id) != v.end();
}

} // namespace

// ── Construction ────────────────────────────────────────────

TEST(SystemGraph, AddSystemReturnsSequentialIds)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    auto b = g.add_system(dod::System{"b", []() {}});
    auto c = g.add_system(dod::System{"c", []() {}});
    EXPECT_EQ(a, 0u);
    EXPECT_EQ(b, 1u);
    EXPECT_EQ(c, 2u);
    EXPECT_EQ(g.size(), 3u);
}

TEST(SystemGraph, AccessSystemByNodeId)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"reader", [](dod::Read<Position>) {}});
    EXPECT_EQ(g.system(a).name(), "reader");
}

TEST(SystemGraph, InvalidNodeIdAsserts)
{
    dod::SystemGraph g;
    EXPECT_DEATH({ (void)g.system(99); }, ".*");
}

// ── Conflict detection ──────────────────────────────────────

TEST(SystemGraph, NoConflictsLeavesAllRoots)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"read_pos", [](dod::Read<Position>) {}});
    auto b = g.add_system(dod::System{"read_vel", [](dod::Read<Velocity>) {}});
    EXPECT_TRUE(g.build());

    EXPECT_EQ(g.roots().size(), 2u);
    EXPECT_TRUE(contains(g.roots(), a));
    EXPECT_TRUE(contains(g.roots(), b));
    EXPECT_EQ(g.dependency_count(a), 0u);
    EXPECT_EQ(g.dependency_count(b), 0u);
}

TEST(SystemGraph, ParallelReadsOnSameComponentDoNotConflict)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"r1", [](dod::Read<Position>) {}});
    g.add_system(dod::System{"r2", [](dod::Read<Position>) {}});
    EXPECT_TRUE(g.build());
    EXPECT_EQ(g.roots().size(), 2u);
}

TEST(SystemGraph, WriteAfterReadConflict)
{
    dod::SystemGraph g;
    auto reader = g.add_system(dod::System{"reader", [](dod::Read<Position>) {}});
    auto writer = g.add_system(dod::System{"writer", [](dod::Write<Position>) {}});
    EXPECT_TRUE(g.build());

    EXPECT_EQ(g.dependency_count(reader), 0u);
    EXPECT_EQ(g.dependency_count(writer), 1u);
    EXPECT_TRUE(contains(g.dependencies(writer), reader));
    EXPECT_TRUE(contains(g.dependents(reader), writer));
    EXPECT_EQ(g.roots().size(), 1u);
    EXPECT_TRUE(contains(g.roots(), reader));
}

TEST(SystemGraph, TwoWritersConflict)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"w1", [](dod::Write<Position>) {}});
    auto b = g.add_system(dod::System{"w2", [](dod::Write<Position>) {}});
    EXPECT_TRUE(g.build());
    EXPECT_TRUE(contains(g.dependencies(b), a));
}

TEST(SystemGraph, DifferentComponentsDoNotConflict)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"w_pos", [](dod::Write<Position>) {}});
    g.add_system(dod::System{"w_vel", [](dod::Write<Velocity>) {}});
    EXPECT_TRUE(g.build());
    EXPECT_EQ(g.roots().size(), 2u);
}

TEST(SystemGraph, WorldWriteIsExclusive)
{
    dod::SystemGraph g;
    auto reader = g.add_system(dod::System{"reader", [](dod::Read<Position>) {}});
    auto exclusive = g.add_system(dod::System{"exclusive", [](dod::WorldWrite) {}});
    auto writer = g.add_system(dod::System{"writer", [](dod::Write<Velocity>) {}});
    EXPECT_TRUE(g.build());

    // exclusive depends on reader; writer depends on exclusive
    EXPECT_TRUE(contains(g.dependencies(exclusive), reader));
    EXPECT_TRUE(contains(g.dependencies(writer), exclusive));
}

TEST(SystemGraph, WorldReadConflictsWithWrites)
{
    dod::SystemGraph g;
    auto observer = g.add_system(dod::System{"observer", [](dod::WorldRead) {}});
    auto writer = g.add_system(dod::System{"writer", [](dod::Write<Position>) {}});
    EXPECT_TRUE(g.build());
    EXPECT_TRUE(contains(g.dependencies(writer), observer));
}

TEST(SystemGraph, WorldReadDoesNotConflictWithReads)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"observer", [](dod::WorldRead) {}});
    g.add_system(dod::System{"reader", [](dod::Read<Position>) {}});
    EXPECT_TRUE(g.build());
    EXPECT_EQ(g.roots().size(), 2u);
}

TEST(SystemGraph, RegistrationOrderDeterminesEdgeDirection)
{
    dod::SystemGraph g;
    auto first = g.add_system(dod::System{"first", [](dod::Write<Position>) {}});
    auto second = g.add_system(dod::System{"second", [](dod::Write<Position>) {}});
    auto third = g.add_system(dod::System{"third", [](dod::Write<Position>) {}});
    EXPECT_TRUE(g.build());

    // Chain: first -> second -> third
    EXPECT_TRUE(contains(g.dependencies(second), first));
    EXPECT_TRUE(contains(g.dependencies(third), first));
    EXPECT_TRUE(contains(g.dependencies(third), second));
    EXPECT_EQ(g.roots().size(), 1u);
    EXPECT_TRUE(contains(g.roots(), first));
}

// ── Explicit ordering ───────────────────────────────────────

TEST(SystemGraph, ExplicitOrderBeforeAddsEdge)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    auto b = g.add_system(dod::System{"b", []() {}});
    g.order_before(a, b);
    EXPECT_TRUE(g.build());

    EXPECT_TRUE(contains(g.dependencies(b), a));
    EXPECT_EQ(g.roots().size(), 1u);
    EXPECT_TRUE(contains(g.roots(), a));
}

TEST(SystemGraph, ExplicitEdgeDuplicatingConflictIsDeduped)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", [](dod::Write<Position>) {}});
    auto b = g.add_system(dod::System{"b", [](dod::Write<Position>) {}});
    g.order_before(a, b); // also implied by conflict
    EXPECT_TRUE(g.build());

    EXPECT_EQ(g.dependencies(b).size(), 1u);
}

TEST(SystemGraph, OrderBeforeOnSelfAsserts)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    EXPECT_DEATH({ g.order_before(a, a); }, ".*");
}

TEST(SystemGraph, OrderBeforeWithInvalidIdAsserts)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    EXPECT_DEATH({ g.order_before(a, 99); }, ".*");
}

TEST(SystemGraph, ExplicitEdgeOverridesRegistrationOrder)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", [](dod::Write<Position>) {}});
    auto b = g.add_system(dod::System{"b", [](dod::Write<Position>) {}});
    g.order_before(b, a); // flip the conflict-derived order
    EXPECT_TRUE(g.build());

    // b should run before a; only b->a edge exists, no cycle.
    EXPECT_TRUE(contains(g.dependencies(a), b));
    EXPECT_EQ(g.dependencies(a).size(), 1u);
    EXPECT_EQ(g.dependencies(b).size(), 0u);
    EXPECT_EQ(g.roots().size(), 1u);
    EXPECT_TRUE(contains(g.roots(), b));
}

// ── Cycle detection ─────────────────────────────────────────

TEST(SystemGraph, ContradictingExplicitEdgesFailBuild)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    auto b = g.add_system(dod::System{"b", []() {}});
    g.order_before(a, b);
    g.order_before(b, a);
    EXPECT_FALSE(g.build());
    EXPECT_FALSE(g.built());
}

TEST(SystemGraph, CycleViaExplicitEdgesFailsBuild)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    auto b = g.add_system(dod::System{"b", []() {}});
    auto c = g.add_system(dod::System{"c", []() {}});
    g.order_before(a, b);
    g.order_before(b, c);
    g.order_before(c, a);

    EXPECT_FALSE(g.build());
    EXPECT_FALSE(g.built());
}

// ── Build state ─────────────────────────────────────────────

TEST(SystemGraph, AddSystemAfterBuildAsserts)
{
    dod::SystemGraph g;
    EXPECT_TRUE(g.build());
    EXPECT_DEATH({ g.add_system(dod::System{"x", []() {}}); }, ".*");
}

TEST(SystemGraph, OrderBeforeAfterBuildAsserts)
{
    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", []() {}});
    auto b = g.add_system(dod::System{"b", []() {}});
    EXPECT_TRUE(g.build());
    EXPECT_DEATH({ g.order_before(a, b); }, ".*");
}

TEST(SystemGraph, BuildIsIdempotent)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"a", [](dod::Write<Position>) {}});
    g.add_system(dod::System{"b", [](dod::Write<Position>) {}});
    EXPECT_TRUE(g.build());
    auto roots_after_first = g.roots();
    EXPECT_TRUE(g.build());
    EXPECT_EQ(g.roots(), roots_after_first);
}

TEST(SystemGraph, EmptyGraphBuildsCleanly)
{
    dod::SystemGraph g;
    EXPECT_TRUE(g.build());
    EXPECT_EQ(g.size(), 0u);
    EXPECT_TRUE(g.roots().empty());
}

// ── Movability ──────────────────────────────────────────────

TEST(SystemGraph, MovedFromGraphReportsUnbuilt)
{
    dod::SystemGraph src;
    src.add_system(dod::System{"a", []() {}});
    EXPECT_TRUE(src.build());
    ASSERT_TRUE(src.built());

    dod::SystemGraph dst = std::move(src);
    EXPECT_TRUE(dst.built());
    EXPECT_FALSE(src.built()); // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(src.size(), 0u);  // NOLINT(bugprone-use-after-move)
}

TEST(SystemGraph, MoveAssignmentResetsSourceBuiltFlag)
{
    dod::SystemGraph src;
    src.add_system(dod::System{"a", []() {}});
    EXPECT_TRUE(src.build());

    dod::SystemGraph dst;
    dst = std::move(src);
    EXPECT_TRUE(dst.built());
    EXPECT_FALSE(src.built()); // NOLINT(bugprone-use-after-move)
}

TEST(SystemGraph, MovableAfterBuild)
{
    dod::SystemGraph g1;
    auto a = g1.add_system(dod::System{"a", [](dod::Write<Position>) {}});
    g1.add_system(dod::System{"b", [](dod::Write<Position>) {}});
    EXPECT_TRUE(g1.build());

    dod::SystemGraph g2 = std::move(g1);
    EXPECT_EQ(g2.size(), 2u);
    EXPECT_EQ(g2.system(a).name(), "a");
}
