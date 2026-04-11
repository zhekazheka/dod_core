#include <dod_core/entity.hpp>
#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>
#include <set>
#include <type_traits>

TEST(Entity, DefaultConstructsAsNull)
{
    dod::Entity e;
    EXPECT_FALSE(e.valid());
    EXPECT_FALSE(static_cast<bool>(e));
}

TEST(Entity, NullStaticMethod)
{
    auto e = dod::Entity::null();
    EXPECT_FALSE(e.valid());
    EXPECT_EQ(e, dod::Entity{});
}

TEST(Entity, ConstructFromEnttEntity)
{
    entt::registry reg;
    auto raw = reg.create();
    dod::Entity e{raw};
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(static_cast<entt::entity>(e), raw);
}

TEST(Entity, RawReturnsIntegralValue)
{
    entt::registry reg;
    auto raw = reg.create();
    dod::Entity e{raw};
    EXPECT_EQ(e.raw(), static_cast<std::uint32_t>(entt::to_integral(raw)));
}

TEST(Entity, ExplicitBoolConversion)
{
    dod::Entity null;
    EXPECT_FALSE(static_cast<bool>(null));

    entt::registry reg;
    dod::Entity valid{reg.create()};
    EXPECT_TRUE(static_cast<bool>(valid));
}

TEST(Entity, EqualityComparison)
{
    entt::registry reg;
    dod::Entity a{reg.create()};
    dod::Entity b{reg.create()};
    dod::Entity a_copy = a;

    EXPECT_EQ(a, a_copy);
    EXPECT_NE(a, b);
}

TEST(Entity, OrderingComparison)
{
    entt::registry reg;
    dod::Entity a{reg.create()};
    dod::Entity b{reg.create()};

    // Just verify ordering is consistent and works
    EXPECT_TRUE((a <=> b) != 0);
    EXPECT_TRUE((a < b) || (b < a));
}

TEST(Entity, UsableInSortedContainer)
{
    entt::registry reg;
    dod::Entity a{reg.create()};
    dod::Entity b{reg.create()};
    dod::Entity c{reg.create()};

    std::set<dod::Entity> entities;
    entities.insert(c);
    entities.insert(a);
    entities.insert(b);
    EXPECT_EQ(entities.size(), 3u);
}

TEST(Entity, ImplicitConversionToEnttEntity)
{
    entt::registry reg;
    auto raw = reg.create();
    dod::Entity e{raw};

    // Should compile: implicit conversion to entt::entity
    entt::entity converted = e;
    EXPECT_EQ(converted, raw);
}

TEST(Entity, CopySemantics)
{
    entt::registry reg;
    dod::Entity original{reg.create()};

    dod::Entity copied{original};
    EXPECT_EQ(copied, original);

    dod::Entity assigned;
    assigned = original;
    EXPECT_EQ(assigned, original);
}

TEST(Entity, TriviallyCopyableAndCorrectSize)
{
    static_assert(std::is_trivially_copyable_v<dod::Entity>);
    static_assert(sizeof(dod::Entity) == sizeof(entt::entity));
}
