#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace dod
{

class ThreadPool
{
  public:
    explicit ThreadPool(std::size_t worker_count = default_worker_count());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    [[nodiscard]] std::size_t worker_count() const noexcept { return m_workers.size(); }

    // Fire-and-forget. Tasks must not throw — exceptions are disabled at the
    // project level. A throwing task will call std::terminate.
    void submit_detached(std::function<void()> task);

    // Pop and execute one queued task on the calling thread. Returns false if
    // the queue was empty. Lets a thread waiting on submitted work help drain
    // the queue instead of idling (the scheduler does this during run()).
    bool try_run_one();

  private:
    static std::size_t default_worker_count() noexcept;
    void worker_loop(std::stop_token st);
    void enqueue(std::function<void()> task);

    std::vector<std::jthread> m_workers;
    std::queue<std::function<void()>> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable_any m_cv;
};

} // namespace dod
