#include "ClockMath.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void ExpectNear(double actual, double expected, const char* label) {
    if (std::abs(actual - expected) > 0.0001) {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void ExpectEqual(std::uint32_t actual, std::uint32_t expected, const char* label) {
    if (actual != expected) {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    SYSTEMTIME midnight{};
    const auto a = widget::ComputeClockAngles(midnight);
    ExpectNear(a.hour, 0.0, "midnight hour");
    ExpectNear(a.minute, 0.0, "midnight minute");
    ExpectNear(a.second, 0.0, "midnight second");

    SYSTEMTIME sample{};
    sample.wHour = 3;
    sample.wMinute = 15;
    sample.wSecond = 30;
    sample.wMilliseconds = 500;
    const auto b = widget::ComputeClockAngles(sample);
    ExpectNear(b.second, 183.0, "fractional second");
    ExpectNear(b.minute, 93.05, "continuous minute");
    ExpectNear(b.hour, 97.7541666667, "continuous hour");

    SYSTEMTIME noon{};
    noon.wHour = 12;
    const auto c = widget::ComputeClockAngles(noon);
    ExpectNear(c.hour, 0.0, "noon hour");

    ExpectEqual(widget::MillisecondsUntilNextLocalMidnight(midnight), 86'400'000,
                "midnight delay");
    SYSTEMTIME lastMoment{};
    lastMoment.wHour = 23;
    lastMoment.wMinute = 59;
    lastMoment.wSecond = 59;
    lastMoment.wMilliseconds = 999;
    ExpectEqual(widget::MillisecondsUntilNextLocalMidnight(lastMoment), 1, "last millisecond");

    std::cout << "clock math tests passed\n";
    return EXIT_SUCCESS;
}
