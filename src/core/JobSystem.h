#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class JobSystem
{
public:
    JobSystem() = default;

    ~JobSystem()
    {
        Stop();
    }

    void Start(unsigned int threadCount = 0)
    {
        if (!m_workers.empty())
        {
            return;
        }

        if (threadCount == 0)
        {
            const unsigned int hardwareThreads =
                std::thread::hardware_concurrency();

            threadCount =
                hardwareThreads > 1
                    ? hardwareThreads - 1
                    : 1;
        }

        m_stopping = false;

        for (unsigned int i = 0; i < threadCount; ++i)
        {
            m_workers.emplace_back(
                [this]()
                {
                    WorkerLoop();
                }
            );
        }
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }

        m_workAvailable.notify_all();

        for (std::thread& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        m_workers.clear();
    }

    template <typename Function>
    void Submit(Function&& function)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_jobs.emplace(std::forward<Function>(function));
            ++m_pendingJobs;
        }

        m_workAvailable.notify_one();
    }

    void Wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_allJobsFinished.wait(
            lock,
            [this]()
            {
                return m_pendingJobs == 0;
            }
        );
    }

    unsigned int GetWorkerCount() const
    {
        return static_cast<unsigned int>(m_workers.size());
    }

private:
    void WorkerLoop()
    {
        while (true)
        {
            std::function<void()> job;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_workAvailable.wait(
                    lock,
                    [this]()
                    {
                        return m_stopping || !m_jobs.empty();
                    }
                );

                if (m_stopping && m_jobs.empty())
                {
                    return;
                }

                job = std::move(m_jobs.front());
                m_jobs.pop();
            }

            job();

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                --m_pendingJobs;

                if (m_pendingJobs == 0)
                {
                    m_allJobsFinished.notify_all();
                }
            }
        }
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_jobs;

    mutable std::mutex m_mutex;
    std::condition_variable m_workAvailable;
    std::condition_variable m_allJobsFinished;

    bool m_stopping = false;
    std::size_t m_pendingJobs = 0;
};