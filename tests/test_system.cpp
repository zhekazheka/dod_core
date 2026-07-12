#include <dod_core/system.hpp>
#include <entt/core/type_info.hpp>
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

// ── ResourceAccess extraction ───────────────────────────────

TEST(System, ExtractsReadAccess)
{
    dod::System sys{"reader", [](dod::View<const Position>) {}};
    EXPECT_EQ(sys.access().reads.size(), 1u);
    EXPECT_EQ(sys.access().writes.size(), 0u);
    EXPECT_EQ(sys.access().reads[0], entt::type_hash<Position>::value());
    EXPECT_FALSE(sys.access().world_read);
    EXPECT_FALSE(sys.access().world_write);
}

TEST(System, ExtractsWriteAccess)
{
    dod::System sys{"writer", [](dod::View<Velocity>) {}};
    EXPECT_EQ(sys.access().reads.size(), 0u);
    EXPECT_EQ(sys.access().writes.size(), 1u);
    EXPECT_EQ(sys.access().writes[0], entt::type_hash<Velocity>::value());
}

TEST(System, ExtractsMixedAccess)
{
    dod::System sys{"mixed",
                    [](dod::View<const Position>, dod::View<Velocity>, dod::View<const Health>) {}};
    EXPECT_EQ(sys.access().reads.size(), 2u);
    EXPECT_EQ(sys.access().writes.size(), 1u);
}

TEST(System, ExtractsMultiComponentViewAccess)
{
    dod::System sys{"integrate", [](dod::View<Position, const Velocity>) {}};
    ASSERT_EQ(sys.access().reads.size(), 1u);
    ASSERT_EQ(sys.access().writes.size(), 1u);
    EXPECT_EQ(sys.access().reads[0], entt::type_hash<Velocity>::value());
    EXPECT_EQ(sys.access().writes[0], entt::type_hash<Position>::value());
}

TEST(System, AliasedWriteAcrossParametersAsserts)
{
    // Position is written by the first parameter and read by the second;
    // nothing orders the two accesses within one system.
    auto make_aliased = []
    { return dod::System{"aliased", [](dod::View<Position>, dod::View<const Position>) {}}; };
    EXPECT_DEATH({ auto sys = make_aliased(); }, ".*");
}

TEST(System, DuplicateReadsAreAllowed)
{
    dod::System sys{"double_read", [](dod::View<const Position>, dod::View<const Position>) {}};
    EXPECT_EQ(sys.access().reads.size(), 2u);
}

TEST(System, ExtractsWorldRead)
{
    dod::System sys{"observer", [](dod::WorldRead) {}};
    EXPECT_TRUE(sys.access().world_read);
    EXPECT_FALSE(sys.access().world_write);
}

TEST(System, ExtractsWorldWrite)
{
    dod::System sys{"mutator", [](dod::WorldWrite) {}};
    EXPECT_FALSE(sys.access().world_read);
    EXPECT_TRUE(sys.access().world_write);
}

TEST(System, NoArgsHasEmptyAccess)
{
    dod::System sys{"empty", []() {}};
    EXPECT_EQ(sys.access().reads.size(), 0u);
    EXPECT_EQ(sys.access().writes.size(), 0u);
    EXPECT_FALSE(sys.access().world_read);
    EXPECT_FALSE(sys.access().world_write);
}

// ── Name ────────────────────────────────────────────────────

TEST(System, StoresName)
{
    dod::System sys{"my_system", []() {}};
    EXPECT_EQ(sys.name(), "my_system");
}

// ── Invocation ──────────────────────────────────────────────

TEST(System, InvokesNoArgFunction)
{
    int counter = 0;
    dod::System sys{"counter", [&counter]() { ++counter; }};

    dod::World world;
    sys(world);
    sys(world);
    EXPECT_EQ(counter, 2);
}

TEST(System, InvokesReadSystem)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 3.0f, 4.0f);

    float captured = 0.0f;
    dod::System sys{"sample_x", [&captured](dod::View<const Position> r)
                    { r.each([&](const Position& p) { captured = p.x; }); }};
    sys(world);
    EXPECT_FLOAT_EQ(captured, 3.0f);
}

TEST(System, InvokesWriteSystem)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);

    dod::System sys{"shift", [](dod::View<Position> w)
                    {
                        w.each(
                            [](Position& p)
                            {
                                p.x += 1.0f;
                                p.y += 2.0f;
                            });
                    }};
    sys(world);
    sys(world);

    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 2.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e).y, 4.0f);
}

TEST(System, InvokesMultiComponentView)
{
    dod::World world;
    auto e = world.create();
    world.emplace<Position>(e, 0.0f, 0.0f);
    world.emplace<Velocity>(e, 5.0f, -3.0f);

    dod::System sys{"integrate", [](dod::View<Position, const Velocity> q)
                    {
                        q.each(
                            [](Position& p, const Velocity& v)
                            {
                                p.x += v.vx;
                                p.y += v.vy;
                            });
                    }};
    sys(world);

    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 5.0f);
    EXPECT_FLOAT_EQ(world.get<Position>(e).y, -3.0f);
}

TEST(System, InvokesWorldWriteSystem)
{
    dod::World world;
    dod::System sys{"spawn", [](dod::WorldWrite ww)
                    {
                        auto e = ww->create();
                        ww->emplace<Health>(e, 100);
                    }};
    sys(world);
    sys(world);

    int total_entities = 0;
    world.view<Health>().each([&total_entities]([[maybe_unused]] const Health& h)
                              { ++total_entities; });
    EXPECT_EQ(total_entities, 2);
}

TEST(System, FreeFunctionPointer)
{
    static int call_count = 0;
    struct Helper
    {
        static void increment(dod::View<const Position>) { ++call_count; }
    };
    call_count = 0;

    dod::System sys{"free_fn", &Helper::increment};
    EXPECT_EQ(sys.access().reads.size(), 1u);

    dod::World world;
    sys(world);
    EXPECT_EQ(call_count, 1);
}

TEST(System, NoexceptFreeFunction)
{
    static int call_count = 0;
    struct Helper
    {
        static void increment(dod::View<Health>) noexcept { ++call_count; }
    };
    call_count = 0;

    dod::System sys{"noexcept_fn", Helper::increment};
    EXPECT_EQ(sys.access().writes.size(), 1u);

    dod::World world;
    sys(world);
    EXPECT_EQ(call_count, 1);
}
