#include <algorithm>
#include <dod_core/thread_pool.hpp>
#include <utility>

namespace dod
{

ThreadPool::ThreadPool(std::size_t worker_count)
{
    const std::size_t n = std::max<std::size_t>(1, worker_count);
    m_workers.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
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

std::future<void> ThreadPool::submit(std::function<void()> task)
{
    auto pt = std::make_shared<std::packaged_task<void()>>(std::move(task));
    std::future<void> fut = pt->get_future();
    enqueue([pt]() { (*pt)(); });
    return fut;
}

void ThreadPool::submit_detached(std::function<void()> task)
{
    enqueue(std::move(task));
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
        try
        {
            task();
        }
        catch (...)
        {
            // Detached tasks have no future; swallow to keep the worker alive.
            // submit() wraps tasks in packaged_task which captures exceptions
            // into the future, so this only fires for submit_detached.
        }
    }
}

} // namespace dod
