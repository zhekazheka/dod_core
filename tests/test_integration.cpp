#include <atomic>
#include <dod_core/dod_core.hpp>
#include <gtest/gtest.h>
#include <vector>

// A small "game loop" exercising the umbrella header end-to-end:
// PreUpdate spawns/initializes, Update simulates, PostUpdate observes.

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

struct Spawned
{
    int frame;
};

namespace
{

dod::SystemGraph build_pre_update(std::atomic<int>& frame_counter)
{
    dod::SystemGraph g;
    g.add_system(dod::System{"advance_frame", [&frame_counter](dod::WorldWrite ww)
                             {
                                 const int frame = frame_counter.fetch_add(1) + 1;
                                 (void)ww;
                                 (void)frame;
                             }});
    EXPECT_TRUE(g.build());
    return g;
}

dod::SystemGraph build_update()
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
    EXPECT_TRUE(g.build());
    return g;
}

dod::SystemGraph build_post_update(std::atomic<int>& observed)
{
    dod::SystemGraph g;
    g.add_system(
        dod::System{"observe", [&observed](dod::Read<Position> pos)
                    { pos.each([&observed](const Position&) { observed.fetch_add(1); }); }});
    EXPECT_TRUE(g.build());
    return g;
}

} // namespace

TEST(Integration, FullGameLoopUpdatesEntitiesAcrossFrames)
{
    std::atomic<int> frame_counter{0};
    std::atomic<int> observations{0};

    dod::Schedule schedule;
    schedule.add_phase(dod::Phase{"PreUpdate", build_pre_update(frame_counter)});
    schedule.add_phase(dod::Phase{"Update", build_update()});
    schedule.add_phase(dod::Phase{"PostUpdate", build_post_update(observations)});

    dod::World world;
    const int entity_count = 100;
    std::vector<dod::Entity> entities;
    entities.reserve(entity_count);
    for (int i = 0; i < entity_count; ++i)
    {
        auto e = world.create();
        world.emplace<Position>(e, 0.0f, 0.0f);
        world.emplace<Velocity>(e, 1.0f, -1.0f);
        entities.push_back(e);
    }

    constexpr int frames = 10;
    for (int f = 0; f < frames; ++f)
    {
        schedule.run(world);
    }

    EXPECT_EQ(frame_counter.load(), frames);
    EXPECT_EQ(observations.load(), entity_count * frames);

    for (auto e : entities)
    {
        EXPECT_FLOAT_EQ(world.get<Position>(e).x, static_cast<float>(frames));
        EXPECT_FLOAT_EQ(world.get<Position>(e).y, -static_cast<float>(frames));
    }
}

TEST(Integration, IndependentSystemsRunInParallelAcrossPhases)
{
    // PreUpdate: 4 sleeping systems that should run in parallel (no conflicts).
    // Verifies the umbrella + Schedule + parallelism all wire together.
    using namespace std::chrono;
    constexpr auto sleep_dur = milliseconds{30};

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

    EXPECT_LT(elapsed.count(), 100);
}

TEST(Integration, ConflictingSystemsRunInOrderAcrossManyFrames)
{
    // Two systems writing the same component must run in sequence.
    // Verify final state is consistent across many frames.
    dod::SystemGraph g;
    g.add_system(dod::System{"increment_x", [](dod::Write<Position> pos)
                             { pos.each([](Position& p) { p.x += 1.0f; }); }});
    g.add_system(dod::System{"double_x", [](dod::Write<Position> pos)
                             { pos.each([](Position& p) { p.x *= 2.0f; }); }});
    EXPECT_TRUE(g.build());

    dod::Schedule s;
    s.add_phase(dod::Phase{"update", std::move(g)});

    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);

    // Each frame: x = (x + 1) * 2
    // After frame 1: (0+1)*2 = 2
    // After frame 2: (2+1)*2 = 6
    // After frame 3: (6+1)*2 = 14
    s.run(world);
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 2.0f);
    s.run(world);
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 6.0f);
    s.run(world);
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 14.0f);
}
