// View iteration vs raw EnTT: the wrapper claims to be a zero-cost
// translation layer, so dod::View and the equivalent entt view must be
// indistinguishable. Any gap here is a bug in the wrapper.
#include <benchmark/benchmark.h>
#include <dod_core/dod_core.hpp>

namespace
{

struct Position
{
    float x, y;
};

struct Velocity
{
    float vx, vy;
};

constexpr std::size_t entity_count = 100'000;

dod::World make_world()
{
    dod::World world;
    for (std::size_t i = 0; i < entity_count; ++i)
    {
        auto e = world.create();
        world.emplace<Position>(e, 1.0f, 2.0f);
        // Every other entity also has Velocity: the join iterates 50k of 100k.
        if (i % 2 == 0)
        {
            world.emplace<Velocity>(e, 0.5f, 0.25f);
        }
    }
    return world;
}

void BM_RawEnttSingle(benchmark::State& state)
{
    auto world = make_world();
    for (auto _ : state)
    {
        world.registry().view<Position>().each([](Position& p) { p.x += 1.0f; });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(entity_count));
}

void BM_DodViewSingle(benchmark::State& state)
{
    auto world = make_world();
    for (auto _ : state)
    {
        dod::View<Position> view{world};
        view.each([](Position& p) { p.x += 1.0f; });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(entity_count));
}

void BM_RawEnttJoin(benchmark::State& state)
{
    auto world = make_world();
    for (auto _ : state)
    {
        world.registry().view<Position, const Velocity>().each(
            [](Position& p, const Velocity& v)
            {
                p.x += v.vx;
                p.y += v.vy;
            });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(entity_count / 2));
}

void BM_DodViewJoin(benchmark::State& state)
{
    auto world = make_world();
    for (auto _ : state)
    {
        dod::View<Position, const Velocity> view{world};
        view.each(
            [](Position& p, const Velocity& v)
            {
                p.x += v.vx;
                p.y += v.vy;
            });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(entity_count / 2));
}

} // namespace

BENCHMARK(BM_RawEnttSingle);
BENCHMARK(BM_DodViewSingle);
BENCHMARK(BM_RawEnttJoin);
BENCHMARK(BM_DodViewJoin);
