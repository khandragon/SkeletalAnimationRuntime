#pragma once

#include <chrono>

class CpuTimer
{
public:
    void Start()
    {
        m_start = Clock::now();
    }

    double StopMilliseconds() const
    {
        const auto end = Clock::now();

        return std::chrono::duration<double, std::milli>(
                   end - m_start)
            .count();
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point m_start = Clock::now();
};

struct ScopedTimer
{
    using Clock = std::chrono::high_resolution_clock;

    const char *name = "";
    double *outputMs = nullptr;
    Clock::time_point start;

    ScopedTimer(const char *timerName, double *output)
        : name(timerName),
          outputMs(output),
          start(Clock::now())
    {
    }

    ~ScopedTimer()
    {
        if (outputMs == nullptr)
        {
            return;
        }

        const auto end = Clock::now();

        *outputMs = std::chrono::duration<double, std::milli>(
                        end - start)
                        .count();
    }

    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;
};