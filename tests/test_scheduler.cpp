#include <atomic>
#include <chrono>
#include <dod_core/scheduler.hpp>
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

dod::SystemGraph make_built_graph()
{
    dod::SystemGraph g;
    EXPECT_TRUE(g.build());
    return g;
}

} // namespace

// ── Empty / construction ────────────────────────────────────

TEST(Scheduler, EmptyGraphRunsCleanly)
{
    dod::Scheduler s{make_built_graph()};
    dod::World world;
    s.run(world);
    SUCCEED();
}

TEST(Scheduler, ConstructionRejectsUnbuiltGraph)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"a", []() {}});
    EXPECT_DEATH({ dod::Scheduler s{std::move(g)}; }, ".*");
}

// ── Basic execution ─────────────────────────────────────────

TEST(Scheduler, RunsAllSystemsOnce)
{
    std::atomic<int> counter{0};
    dod::SystemGraph g;
    g.add_system(dod::System{"a", [&]() { counter.fetch_add(1); }});
    g.add_system(dod::System{"b", [&]() { counter.fetch_add(1); }});
    g.add_system(dod::System{"c", [&]() { counter.fetch_add(1); }});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g)};
    dod::World world;
    s.run(world);
    EXPECT_EQ(counter.load(), 3);
}

TEST(Scheduler, MultipleRunsExecuteEverySystemEachTime)
{
    std::atomic<int> counter{0};
    dod::SystemGraph g;
    g.add_system(dod::System{"a", [&]() { counter.fetch_add(1); }});
    g.add_system(dod::System{"b", [&]() { counter.fetch_add(1); }});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g)};
    dod::World world;
    s.run(world);
    s.run(world);
    s.run(world);
    EXPECT_EQ(counter.load(), 6);
}

// ── Ordering ────────────────────────────────────────────────

TEST(Scheduler, RespectsExplicitOrdering)
{
    std::vector<int> order;
    std::mutex order_mutex;
    auto record = [&](int n)
    {
        std::lock_guard lk(order_mutex);
        order.push_back(n);
    };

    dod::SystemGraph g;
    auto a = g.add_system(dod::System{"a", [&]() { record(1); }});
    auto b = g.add_system(dod::System{"b", [&]() { record(2); }});
    auto c = g.add_system(dod::System{"c", [&]() { record(3); }});
    g.order_before(a, b);
    g.order_before(b, c);
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g)};
    dod::World world;
    s.run(world);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(Scheduler, RespectsConflictDerivedOrdering)
{
    std::vector<int> order;
    std::mutex order_mutex;
    auto record = [&](int n)
    {
        std::lock_guard lk(order_mutex);
        order.push_back(n);
    };

    dod::SystemGraph g;
    g.add_system(dod::System{"first_writer", [&](dod::View<Position>) { record(1); }});
    g.add_system(dod::System{"second_writer", [&](dod::View<Position>) { record(2); }});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g)};
    dod::World world;
    s.run(world);

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

TEST(Scheduler, LongSerialChainRunsInOrder)
{
    // 32 systems all writing Position: a pure conflict-derived chain. This
    // exercises the dispatcher's inline-continuation path (each hop continues
    // on the same thread rather than round-tripping through the queue).
    constexpr int chain_length = 32;
    std::vector<int> order;
    std::mutex order_mutex;

    dod::SystemGraph g;
    for (int i = 0; i < chain_length; ++i)
    {
        g.add_system(dod::System{"link", [&order, &order_mutex, i](dod::View<Position>)
                                 {
                                     std::lock_guard lk(order_mutex);
                                     order.push_back(i);
                                 }});
    }
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g), 4};
    dod::World world;
    s.run(world);
    s.run(world);

    ASSERT_EQ(order.size(), 2u * chain_length);
    for (int i = 0; i < 2 * chain_length; ++i)
    {
        EXPECT_EQ(order[static_cast<std::size_t>(i)], i % chain_length);
    }
}

// ── World mutation ──────────────────────────────────────────

TEST(Scheduler, MutatesWorldAcrossSystems)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"integrate", [](dod::View<const Velocity> vel, dod::View<Position> pos)
                             {
                                 pos.each(
                                     [&](dod::Entity e, Position& p)
                                     {
                                         const auto& v = vel.get(e);
                                         p.x += v.vx;
                                         p.y += v.vy;
                                     });
                             }});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g)};
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);
    world.emplace<Velocity>(e, 1.0f, -1.0f);

    s.run(world);
    s.run(world);
    s.run(world);

    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 3.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e).y, -3.0f);
}

// ── Parallelism ─────────────────────────────────────────────

TEST(Scheduler, IndependentSystemsRunInParallel)
{
    using namespace std::chrono;
    constexpr auto sleep_dur = milliseconds{40};

    dod::SystemGraph g;
    g.add_system(dod::System{"a", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    g.add_system(dod::System{"b", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    g.add_system(dod::System{"c", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    g.add_system(dod::System{"d", [sleep_dur] { std::this_thread::sleep_for(sleep_dur); }});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g), 4};
    dod::World world;

    auto start = steady_clock::now();
    s.run(world);
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    // Serial would take 160ms; parallel ~40ms. Generous slack for CI.
    EXPECT_LT(elapsed.count(), 130);
}

// Regression: parallel Write systems on a fresh world used to create their
// component pools lazily inside workers, racing on the registry's pool map
// (EnTT pool creation is not thread-safe). The scheduler must pre-create
// every pool the graph touches before dispatching. The race itself is only
// observable under TSan; this test pins the pre-creation behavior.
TEST(Scheduler, PreCreatesComponentPoolsBeforeDispatch)
{
    struct FreshA
    {
        int v;
    };
    struct FreshB
    {
        int v;
    };

    dod::SystemGraph g;
    g.add_system(dod::System{"a", [](dod::View<FreshA> a) { a.each([](FreshA& c) { ++c.v; }); }});
    g.add_system(dod::System{"b", [](dod::View<const FreshB>) {}});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g), 4};
    dod::World world; // fresh world: neither pool exists yet

    s.run(world);

    EXPECT_NE(world.registry().storage(entt::type_hash<FreshA>::value()), nullptr);
    EXPECT_NE(world.registry().storage(entt::type_hash<FreshB>::value()), nullptr);
}

// ── Worker count ────────────────────────────────────────────

TEST(Scheduler, WorkerCountForwardedToPool)
{
    dod::Scheduler s{make_built_graph(), 3};
    EXPECT_EQ(s.worker_count(), 3u);
}

// ── Single-threaded (zero-worker) execution ─────────────────

TEST(Scheduler, ZeroWorkersRunsEverySystemOnCallingThread)
{
    const std::thread::id main_thread = std::this_thread::get_id();
    std::atomic<int> counter{0};
    bool all_on_caller = true;

    dod::SystemGraph g;
    for (int i = 0; i < 8; ++i)
    {
        g.add_system(dod::System{"s", [&]()
                                 {
                                     counter.fetch_add(1);
                                     if (std::this_thread::get_id() != main_thread)
                                     {
                                         all_on_caller = false;
                                     }
                                 }});
    }
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g), 0};
    EXPECT_EQ(s.worker_count(), 0u);

    dod::World world;
    s.run(world);
    EXPECT_EQ(counter.load(), 8);
    EXPECT_TRUE(all_on_caller);
}

TEST(Scheduler, ZeroWorkersRespectsDependencyOrder)
{
    // b writes Position, c reads Position -> c must run after b. With zero
    // workers the cached topological order enforces it.
    std::vector<std::string> order;
    dod::SystemGraph g;
    g.add_system(dod::System{"writer", [&](dod::View<Position>) { order.emplace_back("writer"); }});
    g.add_system(
        dod::System{"reader", [&](dod::View<const Position>) { order.emplace_back("reader"); }});
    EXPECT_TRUE(g.build());

    dod::Scheduler s{std::move(g), 0};
    dod::World world;
    s.run(world);

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "writer");
    EXPECT_EQ(order[1], "reader");
}

TEST(SystemGraph, TopologicalOrderCoversAllNodesAfterBuild)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"a", [](dod::View<Position>) {}});
    g.add_system(dod::System{"b", [](dod::View<const Position>) {}});
    g.add_system(dod::System{"c", []() {}});
    ASSERT_TRUE(g.build());

    const auto& order = g.topological_order();
    ASSERT_EQ(order.size(), 3u);
    // Every node appears exactly once, and dependencies precede dependents.
    std::vector<std::size_t> position(3);
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        position[order[i]] = i;
    }
    for (dod::NodeId node = 0; node < 3; ++node)
    {
        for (dod::NodeId dep : g.dependencies(node))
        {
            EXPECT_LT(position[dep], position[node]);
        }
    }
}
