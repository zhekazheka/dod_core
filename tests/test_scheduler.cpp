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
    g.build();
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
    g.build();

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
    g.build();

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
    g.build();

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
    g.add_system(dod::System{"first_writer", [&](dod::Write<Position>) { record(1); }});
    g.add_system(dod::System{"second_writer", [&](dod::Write<Position>) { record(2); }});
    g.build();

    dod::Scheduler s{std::move(g)};
    dod::World world;
    s.run(world);

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

// ── World mutation ──────────────────────────────────────────

TEST(Scheduler, MutatesWorldAcrossSystems)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"integrate", [](dod::Read<Velocity> vel, dod::Write<Position> pos)
                             {
                                 pos.each(
                                     [&](dod::Entity e, Position& p)
                                     {
                                         const auto& v = vel.get(e);
                                         p.x += v.vx;
                                         p.y += v.vy;
                                     });
                             }});
    g.build();

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
    g.build();

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
    g.add_system(dod::System{"a", [](dod::Write<FreshA> a) { a.each([](FreshA& c) { ++c.v; }); }});
    g.add_system(dod::System{"b", [](dod::Read<FreshB>) {}});
    g.build();

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
