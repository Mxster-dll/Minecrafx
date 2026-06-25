#pragma once

#include <windows.h>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

/**
 * @brief 简易线程池 — 支持 parallelRanges 并行任务分发
 *
 * 主线程也参与执行，避免闲置。
 */
class ThreadPool
{
public:
    explicit ThreadPool(int numThreads);
    ~ThreadPool();

    /** @brief 并行执行：将 [0, count) 按 numTasks 份均分，每份调用 func(taskIdx, start, end) */
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
