#pragma once

#include <windows.h>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class ThreadPool
{
public:
    explicit ThreadPool(int numThreads);
    ~ThreadPool();

    void parallelRanges(int count, int numTasks,
        const std::function<void(int, int, int)> &func);

    int workerCount() const;

private:
    void runTasks();
    void workerLoop();

    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop;

    int m_taskCount = 0;
    int m_numTasks = 0;
    std::function<void(int, int, int)> m_taskFunc;
    std::atomic<int> m_nextTask { 0 };
    std::atomic<int> m_pending { 0 };
};