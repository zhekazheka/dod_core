#include <atomic>
#include <chrono>
#include <dod_core/thread_pool.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

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

TEST(ThreadPool, SubmittedTaskRunsAndFutureCompletes)
{
    dod::ThreadPool pool{2};
    std::atomic<bool> ran{false};
    auto fut = pool.submit([&] { ran.store(true); });
    fut.wait();
    EXPECT_TRUE(ran.load());
}

TEST(ThreadPool, ManyTasksAllRun)
{
    dod::ThreadPool pool{4};
    std::atomic<int> counter{0};
    constexpr int N = 1000;

    std::vector<std::future<void>> futures;
    futures.reserve(N);
    for (int i = 0; i < N; ++i)
    {
        futures.push_back(pool.submit([&] { counter.fetch_add(1); }));
    }
    for (auto& f : futures)
    {
        f.wait();
    }
    EXPECT_EQ(counter.load(), N);
}

TEST(ThreadPool, ExceptionInSubmittedTaskPropagatesViaFuture)
{
    dod::ThreadPool pool{1};
    auto fut = pool.submit([] { throw std::runtime_error("boom"); });
    EXPECT_THROW(fut.get(), std::runtime_error);
}

TEST(ThreadPool, DetachedTaskRuns)
{
    dod::ThreadPool pool{1};
    std::atomic<bool> ran{false};
    pool.submit_detached([&] { ran.store(true); });

    // Spin briefly for the task to run; bounded so the test can't hang forever.
    for (int i = 0; i < 100 && !ran.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    EXPECT_TRUE(ran.load());
}

TEST(ThreadPool, DetachedExceptionDoesNotKillWorker)
{
    dod::ThreadPool pool{1};
    pool.submit_detached([] { throw std::runtime_error("boom"); });

    // Pool should still process subsequent work.
    std::atomic<bool> ran{false};
    auto fut = pool.submit([&] { ran.store(true); });
    fut.wait();
    EXPECT_TRUE(ran.load());
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
    std::vector<std::future<void>> futures;
    auto start = steady_clock::now();
    for (int i = 0; i < 4; ++i)
    {
        futures.push_back(pool.submit([sleep_dur] { std::this_thread::sleep_for(sleep_dur); }));
    }
    for (auto& f : futures)
    {
        f.wait();
    }
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    // 4 sleeps of 40ms run in parallel: should finish well under the serial
    // 160ms total. Allow generous slack for slow CI.
    EXPECT_LT(elapsed.count(), 130);
}
