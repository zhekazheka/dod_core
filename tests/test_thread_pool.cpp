#include <dod_core/thread_pool.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{

// Spin-wait until `pred()` returns true or we've waited beyond `timeout_ms`.
// Returns true if the predicate was satisfied within the budget.
template <typename Pred>
bool wait_until(Pred&& pred, int timeout_ms = 1000)
{
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + milliseconds{timeout_ms};
    while (steady_clock::now() < deadline)
    {
        if (pred())
            return true;
        std::this_thread::sleep_for(milliseconds{1});
    }
    return pred();
}

} // namespace

TEST(ThreadPool, WorkerCountReportsCorrectValue)
{
    dod::ThreadPool pool{4};
    EXPECT_EQ(pool.worker_count(), 4u);
}

TEST(ThreadPool, WorkerCountClampsToAtLeastOne)
{
    dod::ThreadPool pool{0};
    EXPECT_GE(pool.worker_count(), 1u);
}

TEST(ThreadPool, DetachedTaskRuns)
{
    dod::ThreadPool pool{1};
    std::atomic<bool> ran{false};
    pool.submit_detached([&] { ran.store(true); });
    EXPECT_TRUE(wait_until([&] { return ran.load(); }));
}

TEST(ThreadPool, ManyDetachedTasksAllRun)
{
    dod::ThreadPool pool{4};
    std::atomic<int> counter{0};
    constexpr int N = 1000;

    for (int i = 0; i < N; ++i)
    {
        pool.submit_detached([&counter] { counter.fetch_add(1); });
    }
    EXPECT_TRUE(wait_until([&] { return counter.load() == N; }));
}

TEST(ThreadPool, DestructorJoinsWorkers)
{
    // Just construct and destroy. jthread auto-joins via stop_token.
    {
        dod::ThreadPool pool{4};
        for (int i = 0; i < 10; ++i)
        {
            pool.submit_detached([] { std::this_thread::sleep_for(std::chrono::milliseconds{1}); });
        }
    }
    SUCCEED();
}

TEST(ThreadPool, ConcurrentExecutionWithMultipleWorkers)
{
    using namespace std::chrono;
    dod::ThreadPool pool{4};

    constexpr auto sleep_dur = milliseconds{40};
    std::atomic<int> done_count{0};

    auto start = steady_clock::now();
    for (int i = 0; i < 4; ++i)
    {
        pool.submit_detached([sleep_dur, &done_count]
                             {
                                 std::this_thread::sleep_for(sleep_dur);
                                 done_count.fetch_add(1);
                             });
    }
    ASSERT_TRUE(wait_until([&] { return done_count.load() == 4; }));
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    // 4 sleeps of 40ms run in parallel: ~40ms, serial would be 160ms.
    EXPECT_LT(elapsed.count(), 130);
}
