#pragma once

#include <windows.h>

#include <cstdint>

namespace widget {

struct ClockAngles {
    double hour;
    double minute;
    double second;
};

ClockAngles ComputeClockAngles(const SYSTEMTIME& localTime) noexcept;
std::uint32_t MillisecondsUntilNextLocalMidnight(const SYSTEMTIME& localTime) noexcept;

} // namespace widget
