// Scheduler dispatch overhead: what does a frame cost before systems do any
// real work? Wide graphs measure per-system queue/wake cost with maximum
// parallelism; the chain measures per-hop latency when every system depends
// on the previous one (worst case: pure serialization through the pool).
#include <benchmark/benchmark.h>
#include <dod_core/dod_core.hpp>

namespace
{

struct Chained
{
    int v;
};

// N independent no-op systems: pure dispatch overhead, fully parallel graph.
void BM_SchedulerWideNoop(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    dod::SystemGraph g;
    for (std::size_t i = 0; i < n; ++i)
    {
        g.add_system(dod::System{"noop", []() {}});
    }
    if (!g.build())
    {
        state.SkipWithError("graph build failed");
        return;
    }
    dod::Scheduler s{std::move(g)};
    dod::World world;

    for (auto _ : state)
    {
        s.run(world);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(n));
}

// N systems all writing the same component: conflict-derived serial chain.
// Each hop is a full "finish system -> enqueue dependent -> worker wakes"
// round trip through the pool.
void BM_SchedulerSerialChain(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    dod::SystemGraph g;
    for (std::size_t i = 0; i < n; ++i)
    {
        g.add_system(dod::System{"link", [](dod::View<Chained>) {}});
    }
    if (!g.build())
    {
        state.SkipWithError("graph build failed");
        return;
    }
    dod::Scheduler s{std::move(g)};
    dod::World world;

    for (auto _ : state)
    {
        s.run(world);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(n));
}

} // namespace

BENCHMARK(BM_SchedulerWideNoop)->Arg(1)->Arg(8)->Arg(32)->Arg(128);
BENCHMARK(BM_SchedulerSerialChain)->Arg(2)->Arg(8)->Arg(32);
