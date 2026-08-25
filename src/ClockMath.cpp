#include "ClockMath.h"

#include <algorithm>

namespace widget {

ClockAngles ComputeClockAngles(const SYSTEMTIME& localTime) noexcept {
    const double seconds = static_cast<double>(localTime.wSecond) +
                           static_cast<double>(localTime.wMilliseconds) / 1000.0;
    const double minutes = static_cast<double>(localTime.wMinute) + seconds / 60.0;
    const double hours = static_cast<double>(localTime.wHour % 12) + minutes / 60.0;

    return ClockAngles{
        hours * 30.0,
        minutes * 6.0,
        seconds * 6.0,
    };
}

std::uint32_t MillisecondsUntilNextLocalMidnight(const SYSTEMTIME& localTime) noexcept {
    const std::uint64_t elapsed =
        ((static_cast<std::uint64_t>(localTime.wHour) * 60ULL + localTime.wMinute) * 60ULL +
         localTime.wSecond) * 1000ULL + localTime.wMilliseconds;
    constexpr std::uint64_t day = 24ULL * 60ULL * 60ULL * 1000ULL;
    const std::uint64_t remaining = std::max<std::uint64_t>(1, day - std::min(elapsed, day - 1));
    return static_cast<std::uint32_t>(remaining);
}

} // namespace widget
