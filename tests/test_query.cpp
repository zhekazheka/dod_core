#include <dod_core/query.hpp>
#include <gtest/gtest.h>

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

// ── Read ────────────────────────────────────────────────────

TEST(Read, IteratesEntitiesWithComponent)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    auto e3 = world.create();
    world.emplace<Position>(e1, 1.0f, 0.0f);
    world.emplace<Position>(e2, 2.0f, 0.0f);
    // e3 has no Position

    dod::Read<Position> reader{world};
    int count = 0;
    for ([[maybe_unused]] auto ent : reader)
    {
        ++count;
    }
    EXPECT_EQ(count, 2);
    EXPECT_TRUE(reader.contains(e1));
    EXPECT_TRUE(reader.contains(e2));
    EXPECT_FALSE(reader.contains(e3));
}

TEST(Read, GetReturnsConstReference)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 7.0f, 8.0f);

    dod::Read<Position> reader{world};
    const Position& pos = reader.get(e);
    EXPECT_FLOAT_EQ(pos.x, 7.0f);
    EXPECT_FLOAT_EQ(pos.y, 8.0f);

    static_assert(std::is_const_v<std::remove_reference_t<decltype(reader.get(e))>>);
}

TEST(Read, EachInvokesCallback)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    world.emplace<Position>(e1, 1.0f, 2.0f);
    world.emplace<Position>(e2, 3.0f, 4.0f);

    dod::Read<Position> reader{world};
    float total = 0.0f;
    reader.each([&](const Position& p) { total += p.x; });
    EXPECT_FLOAT_EQ(total, 4.0f);
}

// ── Write ───────────────────────────────────────────────────

TEST(Write, MutatesComponents)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);

    dod::Write<Position> writer{world};
    writer.get(e).x = 42.0f;
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 42.0f);
}

TEST(Write, EachMutatesAllMatching)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    world.emplace<Velocity>(e1, 1.0f, 0.0f);
    world.emplace<Velocity>(e2, 2.0f, 0.0f);

    dod::Write<Velocity> writer{world};
    writer.each([](Velocity& v) { v.vx *= 10.0f; });

    EXPECT_FLOAT_EQ(world.get<Velocity>(e1).vx, 10.0f);
    EXPECT_FLOAT_EQ(world.get<Velocity>(e2).vx, 20.0f);
}

// ── WorldRead ───────────────────────────────────────────────

TEST(WorldRead, ProvidesConstAccess)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 5.0f, 6.0f);

    dod::WorldRead reader{world};
    EXPECT_TRUE(reader->alive(e));
    EXPECT_FLOAT_EQ(reader->get<Position>(e).x, 5.0f);

    // Verify it's a const reference (can't call non-const methods)
    static_assert(std::is_same_v<decltype(reader.world()), const dod::World&>);
}

// ── WorldWrite ──────────────────────────────────────────────

TEST(WorldWrite, ProvidesMutableAccess)
{
    dod::World world;
    dod::WorldWrite writer{world};
    auto e = writer->create();
    writer->emplace<Position>(e, 1.0f, 2.0f);
    EXPECT_TRUE(world.alive(e));
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 1.0f);
}
