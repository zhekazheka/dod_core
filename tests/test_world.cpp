#include <dod_core/world.hpp>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

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

// ── Entity lifecycle ────────────────────────────────────────

TEST(World, CreateReturnsValidEntity)
{
    dod::World world;
    auto e = world.create();
    EXPECT_TRUE(e.valid());
    EXPECT_TRUE(world.alive(e));
}

TEST(World, DestroyMakesEntityInvalid)
{
    dod::World world;
    auto e = world.create();
    world.destroy(e);
    EXPECT_FALSE(world.alive(e));
}

TEST(World, CreateMultipleEntitiesAreDistinct)
{
    dod::World world;
    auto a = world.create();
    auto b = world.create();
    EXPECT_NE(a, b);
}

// ── Component operations ────────────────────────────────────

TEST(World, EmplaceAndGet)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 1.0f, 2.0f);

    auto& pos = world.get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
}

TEST(World, EmplaceDefaultConstructed)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Health>(e);

    auto& h = world.get<Health>(e);
    h.hp = 100;
    EXPECT_EQ(world.get<Health>(e).hp, 100);
}

TEST(World, GetReturnsMutableReference)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);

    world.get<Position>(e).x = 42.0f;
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 42.0f);
}

TEST(World, GetConstReturnsConstReference)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 5.0f, 10.0f);

    const auto& cworld = world;
    const auto& pos = cworld.get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 5.0f);

    static_assert(std::is_const_v<std::remove_reference_t<decltype(cworld.get<Position>(e))>>);
}

TEST(World, HasReturnsTrueForEmplacedComponent)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);
    EXPECT_TRUE(world.has<Position>(e));
}

TEST(World, HasReturnsFalseForMissingComponent)
{
    dod::World world;
    auto e = world.create();
    EXPECT_FALSE(world.has<Position>(e));
}

TEST(World, HasMultipleComponents)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);
    EXPECT_FALSE((world.has<Position, Velocity>(e)));

    world.emplace<Velocity>(e, 1.0f, 1.0f);
    EXPECT_TRUE((world.has<Position, Velocity>(e)));
}

TEST(World, RemoveComponent)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);
    EXPECT_TRUE(world.has<Position>(e));

    world.remove<Position>(e);
    EXPECT_FALSE(world.has<Position>(e));
}

// ── Views ───────────────────────────────────────────────────

TEST(World, ViewIteratesMatchingEntities)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();
    auto e3 = world.create();

    world.emplace<Position>(e1, 1.0f, 0.0f);
    world.emplace<Position>(e2, 2.0f, 0.0f);
    // e3 has no Position

    std::vector<dod::Entity> found;
    auto v = world.view<Position>();
    for (auto ent : v)
    {
        found.emplace_back(ent);
    }

    EXPECT_EQ(found.size(), 2u);
    EXPECT_TRUE(std::find(found.begin(), found.end(), e1) != found.end());
    EXPECT_TRUE(std::find(found.begin(), found.end(), e2) != found.end());
    EXPECT_TRUE(std::find(found.begin(), found.end(), e3) == found.end());
}

TEST(World, ViewMultipleComponents)
{
    dod::World world;
    auto e1 = world.create();
    auto e2 = world.create();

    world.emplace<Position>(e1, 0.0f, 0.0f);
    world.emplace<Velocity>(e1, 1.0f, 1.0f);
    world.emplace<Position>(e2, 0.0f, 0.0f);
    // e2 has only Position, not Velocity

    int count = 0;
    auto v = world.view<Position, Velocity>();
    for ([[maybe_unused]] auto ent : v)
    {
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(World, ViewEachCallback)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 1.0f, 2.0f);
    world.emplace<Velocity>(e, 3.0f, 4.0f);

    world.view<Position, Velocity>().each(
        [](auto& pos, auto& vel)
        {
            pos.x += vel.vx;
            pos.y += vel.vy;
        });

    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 4.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e).y, 6.0f);
}

// ── Escape hatch ────────────────────────────────────────────

TEST(World, RegistryEscapeHatch)
{
    dod::World world;
    entt::registry& reg = world.registry();
    auto raw = reg.create();
    EXPECT_TRUE(reg.valid(raw));
}

TEST(World, RegistryEscapeHatchConst)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 3.0f, 4.0f);

    const auto& cworld = world;
    const entt::registry& reg = cworld.registry();
    EXPECT_TRUE(reg.valid(e));
    EXPECT_FLOAT_EQ(reg.get<Position>(e).x, 3.0f);
}

// ── Type properties ─────────────────────────────────────────

TEST(World, NonCopyable)
{
    static_assert(!std::is_copy_constructible_v<dod::World>);
    static_assert(!std::is_copy_assignable_v<dod::World>);
}

TEST(World, Movable)
{
    dod::World w1;
    auto e = w1.create();
    w1.emplace<Position>(e, 7.0f, 8.0f);

    dod::World w2 = std::move(w1);
    EXPECT_TRUE(w2.alive(e));
    EXPECT_FLOAT_EQ(w2.get<Position>(e).x, 7.0f);
}

TEST(World, DestroyAndRecreate)
{
    dod::World world;
    auto e1 = world.create();
    world.destroy(e1);
    auto e2 = world.create();
    // EnTT recycles entity IDs, so the raw ID portion may match
    // but the entity handle should be different (version incremented)
    EXPECT_NE(e1, e2);
}
