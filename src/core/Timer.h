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