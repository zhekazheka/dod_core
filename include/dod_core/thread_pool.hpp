#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
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

    // Submit a task and obtain a future. Exceptions thrown by the task are
    // captured and surfaced through future.get().
    [[nodiscard]] std::future<void> submit(std::function<void()> task);

    // Fire-and-forget. Exceptions thrown by the task are swallowed (logging
    // would be the next step; for now we keep workers alive on user errors).
    void submit_detached(std::function<void()> task);

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
