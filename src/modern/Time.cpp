#include <Obol/base/Time.h>

#include <chrono>
#include <cmath>
#include <ctime>

namespace obol {

TimeSpan
TimeSpan::fromSeconds(double value)
{
    TimeSpan span;
    span.seconds = value;
    return span;
}

TimeSpan
TimeSpan::fromMilliseconds(double value)
{
    TimeSpan span;
    span.seconds = value / 1000.0;
    return span;
}

Time::Time(double value)
    : seconds_(value)
{
}

Time
Time::unixEpoch()
{
    return Time(0.0);
}

Time
Time::fromUnixSeconds(double value)
{
    return Time(value);
}

Time
Time::now()
{
    const auto now = std::chrono::system_clock::now();
    const auto sinceEpoch = now.time_since_epoch();
    const std::chrono::duration<double> seconds = sinceEpoch;
    return Time(seconds.count());
}

double
Time::secondsSinceUnixEpoch() const
{
    return seconds_;
}

std::string
Time::formatUTC(const char * format) const
{
    if (!format) {
        return std::string();
    }

    std::time_t wholeSeconds =
        static_cast<std::time_t>(std::floor(seconds_));
    std::tm utc;
#if defined(_WIN32)
    if (gmtime_s(&utc, &wholeSeconds) != 0) {
        return std::string();
    }
#else
    if (!gmtime_r(&wholeSeconds, &utc)) {
        return std::string();
    }
#endif

    char buffer[128];
    const std::size_t written =
        std::strftime(buffer, sizeof(buffer), format, &utc);
    if (written == 0) {
        return std::string();
    }
    return std::string(buffer, written);
}

Time
Time::operator+(TimeSpan span) const
{
    return Time(seconds_ + span.seconds);
}

Time
Time::operator-(TimeSpan span) const
{
    return Time(seconds_ - span.seconds);
}

TimeSpan
Time::operator-(Time other) const
{
    return TimeSpan::fromSeconds(seconds_ - other.seconds_);
}

} // namespace obol
