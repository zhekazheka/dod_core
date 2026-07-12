#include <dod_core/system_traits.hpp>
#include <gtest/gtest.h>

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

// ── Plain free functions ────────────────────────────────────

void read_only_system(dod::View<const Position>) {}
void write_only_system(dod::View<Velocity>) {}
void mixed_system(dod::View<const Position>, dod::View<Velocity>) {}
void world_read_system(dod::WorldRead) {}
void world_write_system(dod::WorldWrite) {}
void no_arg_system() {}

TEST(SystemTraits, ReadOnlyFunction)
{
    using traits = dod::SystemTraits<decltype(&read_only_system)>;
    static_assert(traits::arity == 1);
    static_assert(traits::read_count == 1);
    static_assert(traits::write_count == 0);
    static_assert(!traits::needs_world_read);
    static_assert(!traits::needs_world_write);
    static_assert(!traits::is_exclusive);
}

TEST(SystemTraits, WriteOnlyFunction)
{
    using traits = dod::SystemTraits<decltype(&write_only_system)>;
    static_assert(traits::arity == 1);
    static_assert(traits::read_count == 0);
    static_assert(traits::write_count == 1);
}

TEST(SystemTraits, MixedFunction)
{
    using traits = dod::SystemTraits<decltype(&mixed_system)>;
    static_assert(traits::arity == 2);
    static_assert(traits::read_count == 1);
    static_assert(traits::write_count == 1);
}

TEST(SystemTraits, WorldReadFunction)
{
    using traits = dod::SystemTraits<decltype(&world_read_system)>;
    static_assert(traits::needs_world_read);
    static_assert(!traits::needs_world_write);
    static_assert(!traits::is_exclusive);
}

TEST(SystemTraits, WorldWriteFunctionIsExclusive)
{
    using traits = dod::SystemTraits<decltype(&world_write_system)>;
    static_assert(!traits::needs_world_read);
    static_assert(traits::needs_world_write);
    static_assert(traits::is_exclusive);
}

TEST(SystemTraits, NoArgFunction)
{
    using traits = dod::SystemTraits<decltype(&no_arg_system)>;
    static_assert(traits::arity == 0);
    static_assert(traits::read_count == 0);
    static_assert(traits::write_count == 0);
    static_assert(!traits::needs_world_read);
    static_assert(!traits::needs_world_write);
}

// ── Lambdas ─────────────────────────────────────────────────

TEST(SystemTraits, Lambda)
{
    auto lam = [](dod::View<const Position>, dod::View<Velocity>) {};
    using traits = dod::SystemTraits<decltype(lam)>;
    static_assert(traits::arity == 2);
    static_assert(traits::read_count == 1);
    static_assert(traits::write_count == 1);
}

TEST(SystemTraits, MutableLambda)
{
    auto lam = [counter = 0](dod::View<Position>) mutable { ++counter; };
    using traits = dod::SystemTraits<decltype(lam)>;
    static_assert(traits::arity == 1);
    static_assert(traits::write_count == 1);
}

TEST(SystemTraits, FunctionPointerWithoutAddressOf)
{
    using traits = dod::SystemTraits<void (*)(dod::View<const Position>, dod::View<Velocity>)>;
    static_assert(traits::arity == 2);
    static_assert(traits::read_count == 1);
    static_assert(traits::write_count == 1);
}

// ── noexcept callables ──────────────────────────────────────
// noexcept is part of the function type since C++17; these used to fail to
// compile because function_traits lacked noexcept specializations for free
// functions and function pointers.

void noexcept_system(dod::View<Velocity>) noexcept {}

TEST(SystemTraits, NoexceptFreeFunction)
{
    using traits = dod::SystemTraits<decltype(&noexcept_system)>;
    static_assert(traits::arity == 1);
    static_assert(traits::write_count == 1);
}

TEST(SystemTraits, NoexceptFunctionType)
{
    using traits = dod::SystemTraits<decltype(noexcept_system)>;
    static_assert(traits::arity == 1);
    static_assert(traits::write_count == 1);
}

TEST(SystemTraits, NoexceptLambda)
{
    auto lam = [](dod::View<const Position>) noexcept {};
    using traits = dod::SystemTraits<decltype(lam)>;
    static_assert(traits::arity == 1);
    static_assert(traits::read_count == 1);
}
