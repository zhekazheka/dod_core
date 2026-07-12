// Does the parallel scheduler pay for itself on real component work?
// Four independent systems, each integrating its own 100k-entity component
// set, run through the scheduler vs called back to back on one thread.
// Near-4x on 4+ cores is the win condition; the gap from ideal is the
// scheduler's coordination cost at this work size.
#include <benchmark/benchmark.h>
#include <dod_core/dod_core.hpp>

namespace
{

// Four distinct component types so the four systems are conflict-free.
template <int N> struct Body
{
    float x, v;
};

constexpr std::size_t entities_per_set = 100'000;
constexpr int steps_per_entity = 16; // enough math per entity to dwarf dispatch

template <int N> void integrate(dod::View<Body<N>> view)
{
    view.each(
        [](Body<N>& b)
        {
            for (int i = 0; i < steps_per_entity; ++i)
            {
                b.v += 0.001f * b.x;
                b.x += 0.016f * b.v;
            }
        });
}

dod::World make_world()
{
    dod::World world;
    for (std::size_t i = 0; i < entities_per_set; ++i)
    {
        auto e = world.create();
        world.emplace<Body<0>>(e, 1.0f, 0.0f);
        world.emplace<Body<1>>(e, 1.0f, 0.0f);
        world.emplace<Body<2>>(e, 1.0f, 0.0f);
        world.emplace<Body<3>>(e, 1.0f, 0.0f);
    }
    return world;
}

void BM_FourSystemsSerial(benchmark::State& state)
{
    auto world = make_world();
    dod::System s0{"i0", integrate<0>};
    dod::System s1{"i1", integrate<1>};
    dod::System s2{"i2", integrate<2>};
    dod::System s3{"i3", integrate<3>};

    for (auto _ : state)
    {
        s0(world);
        s1(world);
        s2(world);
        s3(world);
        benchmark::ClobberMemory();
    }
}

void BM_FourSystemsScheduled(benchmark::State& state)
{
    auto world = make_world();
    dod::SystemGraph g;
    g.add_system(dod::System{"i0", integrate<0>});
    g.add_system(dod::System{"i1", integrate<1>});
    g.add_system(dod::System{"i2", integrate<2>});
    g.add_system(dod::System{"i3", integrate<3>});
    if (!g.build())
    {
        state.SkipWithError("graph build failed");
        return;
    }
    dod::Scheduler s{std::move(g), 4};

    for (auto _ : state)
    {
        s.run(world);
        benchmark::ClobberMemory();
    }
}

} // namespace

BENCHMARK(BM_FourSystemsSerial)->UseRealTime();
BENCHMARK(BM_FourSystemsScheduled)->UseRealTime();
