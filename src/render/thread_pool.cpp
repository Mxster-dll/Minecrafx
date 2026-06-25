#include "thread_pool.h"

ThreadPool::ThreadPool(int numThreads)
    : m_stop(false)
{
    for (int i = 0; i < numThreads; ++i)
        m_workers.emplace_back(&ThreadPool::workerLoop, this);
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto &w : m_workers)
        if (w.joinable()) w.join();
}

void ThreadPool::parallelRanges(int count, int numTasks,
    const std::function<void(int, int, int)> &func)
{
    if (count <= 0 || numTasks <= 1)
    {
        func(0, 0, count);
        return;
    }

    m_pending.store(numTasks, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_taskCount = count;
        m_numTasks = numTasks;
        m_taskFunc = func;
        m_nextTask.store(0, std::memory_order_release);
    }
    m_cv.notify_all();

    // 主线程也参与
    runTasks();

    // 等待所有 worker 完成
    while (m_pending.load(std::memory_order_acquire) > 0)
        std::this_thread::yield();
}

int ThreadPool::workerCount() const
{
    return (int) m_workers.size();
}

void ThreadPool::runTasks()
{
    int count = m_taskCount;
    int numTasks = m_numTasks;
    const auto &func = m_taskFunc;
    while (true)
    {
        int t = m_nextTask.fetch_add(1, std::memory_order_acq_rel);
        if (t >= numTasks) break;
        int start = (int) ((long long) count * t / numTasks);
        int end = (int) ((long long) count * (t + 1) / numTasks);
        func(t, start, end);
        m_pending.fetch_sub(1, std::memory_order_acq_rel);
    }
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]
            {
                return m_stop || m_pending.load(std::memory_order_acquire) > 0;
            });
            if (m_stop) return;
        }
        runTasks();
    }
}
