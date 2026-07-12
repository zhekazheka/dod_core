#include <dod_core/thread_pool.hpp>

#include <utility>

namespace dod
{

ThreadPool::ThreadPool(std::size_t worker_count)
{
    // worker_count == 0 is a valid state: a pool with no background threads.
    // submit_detached then only runs via try_run_one() on the caller, and the
    // scheduler takes a fully inline execution path (single-threaded).
    m_workers.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i)
    {
        m_workers.emplace_back([this](std::stop_token st) { worker_loop(st); });
    }
}

ThreadPool::~ThreadPool()
{
    // Request stop so workers waking up see stop_requested == true.
    for (auto& w : m_workers)
    {
        w.request_stop();
    }
    // Wake everyone so they can exit even if the queue is empty.
    m_cv.notify_all();
    // jthread destructors auto-join.
}

void ThreadPool::submit_detached(std::function<void()> task) { enqueue(std::move(task)); }

bool ThreadPool::try_run_one()
{
    std::function<void()> task;
    {
        std::lock_guard lock(m_mutex);
        if (m_queue.empty())
        {
            return false;
        }
        task = std::move(m_queue.front());
        m_queue.pop();
    }
    // Exceptions are disabled at the project level. Tasks must not throw.
    task();
    return true;
}

std::size_t ThreadPool::default_worker_count() noexcept
{
    const auto hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1u : static_cast<std::size_t>(hw);
}

void ThreadPool::enqueue(std::function<void()> task)
{
    {
        std::lock_guard lock(m_mutex);
        m_queue.push(std::move(task));
    }
    m_cv.notify_one();
}

void ThreadPool::worker_loop(std::stop_token st)
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, st, [this] { return !m_queue.empty(); });
            if (m_queue.empty())
            {
                // Stop was requested and queue is drained.
                return;
            }
            task = std::move(m_queue.front());
            m_queue.pop();
        }
        // Exceptions are disabled at the project level. Tasks must not throw.
        task();
    }
}

} // namespace dod
