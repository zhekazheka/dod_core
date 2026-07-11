#include <atomic>
#include <chrono>
#include <dod_core/phase.hpp>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

namespace
{

dod::SystemGraph make_built_empty_graph()
{
    dod::SystemGraph g;
    EXPECT_TRUE(g.build());
    return g;
}

} // namespace

// ── Phase ───────────────────────────────────────────────────

TEST(Phase, StoresNameAndGraph)
{
    dod::Phase p{"update", make_built_empty_graph()};
    EXPECT_EQ(p.name(), "update");
    EXPECT_EQ(p.graph().size(), 0u);
}

TEST(Phase, RejectsUnbuiltGraph)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"a", []() {}});
    EXPECT_DEATH({ dod::Phase p("update", std::move(g)); }, ".*");
}

TEST(Phase, Movable)
{
    dod::Phase a{"a", make_built_empty_graph()};
    dod::Phase b = std::move(a);
    EXPECT_EQ(b.name(), "a");
}

// ── Schedule ────────────────────────────────────────────────

TEST(Schedule, EmptyScheduleRunsCleanly)
{
    dod::Schedule s;
    dod::World world;
    s.run(world);
    EXPECT_EQ(s.phase_count(), 0u);
}

TEST(Schedule, AddPhaseIncrementsPhaseCount)
{
    dod::Schedule s;
    s.add_phase(dod::Phase{"a", make_built_empty_graph()});
    s.add_phase(dod::Phase{"b", make_built_empty_graph()});
    EXPECT_EQ(s.phase_count(), 2u);
    EXPECT_EQ(s.phase(0).name(), "a");
    EXPECT_EQ(s.phase(1).name(), "b");
}

TEST(Schedule, PhaseAccessOutOfRangeThrows)
{
    dod::Schedule s;
    EXPECT_DEATH({ (void)s.phase(0); }, ".*");
}

TEST(Schedule, RunsAllSystemsAcrossPhases)
{
    std::atomic<int> counter{0};

    dod::SystemGraph g1;
    g1.add_system(dod::System{"a", [&]() { counter.fetch_add(1); }});
    g1.add_system(dod::System{"b", [&]() { counter.fetch_add(1); }});
    EXPECT_TRUE(g1.build());

    dod::SystemGraph g2;
    g2.add_system(dod::System{"c", [&]() { counter.fetch_add(1); }});
    EXPECT_TRUE(g2.build());

    dod::Schedule s;
    s.add_phase(dod::Phase{"phase1", std::move(g1)});
    s.add_phase(dod::Phase{"phase2", std::move(g2)});

    dod::World world;
    s.run(world);
    EXPECT_EQ(counter.load(), 3);
}

TEST(Schedule, PhasesExecuteSequentially)
{
    std::vector<int> order;
    std::mutex order_mutex;
    auto record = [&](int n)
    {
        std::lock_guard lk(order_mutex);
        order.push_back(n);
    };

    dod::SystemGraph g1;
    g1.add_system(dod::System{"p1a", [&]() { record(1); }});
    g1.add_system(dod::System{"p1b", [&]() { record(1); }});
    EXPECT_TRUE(g1.build());

    dod::SystemGraph g2;
    g2.add_system(dod::System{"p2a", [&]() { record(2); }});
    g2.add_system(dod::System{"p2b", [&]() { record(2); }});
    EXPECT_TRUE(g2.build());

    dod::Schedule s;
    s.add_phase(dod::Phase{"phase1", std::move(g1)});
    s.add_phase(dod::Phase{"phase2", std::move(g2)});

    dod::World world;
    s.run(world);

    // All phase1 entries must come before all phase2 entries.
    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
    EXPECT_EQ(order[3], 2);
}

TEST(Schedule, SystemsWithinPhaseRunInParallel)
{
    using namespace std::chrono;
    constexpr auto sleep_dur = milliseconds{40};

    dod::SystemGraph g;
    g.add_system(dod::System{"a", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    g.add_system(dod::System{"b", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    g.add_system(dod::System{"c", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    g.add_system(dod::System{"d", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    EXPECT_TRUE(g.build());

    dod::Schedule s{4};
    s.add_phase(dod::Phase{"parallel", std::move(g)});

    dod::World world;
    auto start = steady_clock::now();
    s.run(world);
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    // 4 systems × 40ms serial = 160ms; parallel ≈ 40ms.
    EXPECT_LT(elapsed.count(), 130);
}

TEST(Schedule, MultipleRunsExecuteAllPhasesEachTime)
{
    std::atomic<int> counter{0};

    dod::SystemGraph g;
    g.add_system(dod::System{"a", [&]() { counter.fetch_add(1); }});
    EXPECT_TRUE(g.build());

    dod::Schedule s;
    s.add_phase(dod::Phase{"only", std::move(g)});

    dod::World world;
    s.run(world);
    s.run(world);
    s.run(world);
    EXPECT_EQ(counter.load(), 3);
}
