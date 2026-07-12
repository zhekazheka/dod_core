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

// ── Single-component View ───────────────────────────────────

TEST(View, IteratesEntitiesWithComponent)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    auto e3 = world.create();
    world.emplace<Position>(e1, 1.0f, 0.0f);
    world.emplace<Position>(e2, 2.0f, 0.0f);
    // e3 has no Position

    dod::View<const Position> view{world};
    int count = 0;
    for ([[maybe_unused]] auto ent : view)
    {
        ++count;
    }
    EXPECT_EQ(count, 2);
    EXPECT_TRUE(view.contains(e1));
    EXPECT_TRUE(view.contains(e2));
    EXPECT_FALSE(view.contains(e3));
}

TEST(View, ConstComponentGetReturnsConstReference)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 7.0f, 8.0f);

    dod::View<const Position> view{world};
    const Position& pos = view.get(e);
    EXPECT_FLOAT_EQ(pos.x, 7.0f);
    EXPECT_FLOAT_EQ(pos.y, 8.0f);

    static_assert(std::is_const_v<std::remove_reference_t<decltype(view.get(e))>>);
}

TEST(View, ConstComponentEachInvokesCallback)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    world.emplace<Position>(e1, 1.0f, 2.0f);
    world.emplace<Position>(e2, 3.0f, 4.0f);

    dod::View<const Position> view{world};
    float total = 0.0f;
    view.each([&](const Position& p) { total += p.x; });
    EXPECT_FLOAT_EQ(total, 4.0f);
}

TEST(View, MutableComponentGetMutates)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);

    dod::View<Position> view{world};
    view.get(e).x = 42.0f;
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 42.0f);

    static_assert(!std::is_const_v<std::remove_reference_t<decltype(view.get(e))>>);
}

TEST(View, MutableComponentEachMutatesAllMatching)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    world.emplace<Velocity>(e1, 1.0f, 0.0f);
    world.emplace<Velocity>(e2, 2.0f, 0.0f);

    dod::View<Velocity> view{world};
    view.each([](Velocity& v) { v.vx *= 10.0f; });

    EXPECT_FLOAT_EQ(world.get<Velocity>(e1).vx, 10.0f);
    EXPECT_FLOAT_EQ(world.get<Velocity>(e2).vx, 20.0f);
}

// ── Multi-component View ────────────────────────────────────

TEST(View, MultiComponentIteratesIntersectionOnly)
{
    dod::World world;
    auto both = world.create();
    auto pos_only = world.create();
    auto vel_only = world.create();
    world.emplace<Position>(both, 0.0f, 0.0f);
    world.emplace<Velocity>(both, 1.0f, 1.0f);
    world.emplace<Position>(pos_only, 0.0f, 0.0f);
    world.emplace<Velocity>(vel_only, 1.0f, 1.0f);

    dod::View<Position, const Velocity> view{world};
    int count = 0;
    view.each([&](Position&, const Velocity&) { ++count; });
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(view.contains(both));
    EXPECT_FALSE(view.contains(pos_only));
    EXPECT_FALSE(view.contains(vel_only));
}

TEST(View, MultiComponentEachMixedAccess)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);
    world.emplace<Velocity>(e, 5.0f, -3.0f);

    dod::View<Position, const Velocity> view{world};
    view.each(
        [](Position& p, const Velocity& v)
        {
            p.x += v.vx;
            p.y += v.vy;
        });

    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 5.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e).y, -3.0f);
}

TEST(View, MultiComponentEachWithEntity)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 1.0f, 0.0f);
    world.emplace<Velocity>(e, 0.0f, 0.0f);

    dod::Entity seen = dod::Entity::null();
    dod::View<Position, const Velocity> view{world};
    view.each([&](dod::Entity ent, Position&, const Velocity&) { seen = ent; });
    EXPECT_EQ(seen, dod::Entity{e});
}

TEST(View, MultiComponentGetByType)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 2.0f, 3.0f);
    world.emplace<Velocity>(e, 4.0f, 5.0f);

    dod::View<Position, const Velocity> view{world};
    view.get<Position>(e).x = 20.0f;
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 20.0f);

    const Velocity& v = view.get<const Velocity>(e);
    EXPECT_FLOAT_EQ(v.vx, 4.0f);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(view.get<const Velocity>(e))>>);
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
