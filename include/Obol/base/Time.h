#ifndef OBOL_BASE_TIME_H
#define OBOL_BASE_TIME_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <Obol/base/Export.h>

#include <string>

namespace obol {

struct TimeSpan {
    double seconds = 0.0;

    static TimeSpan fromSeconds(double value);
    static TimeSpan fromMilliseconds(double value);
};

class OBOL_V2_API Time {
public:
    Time() = default;

    static Time unixEpoch();
    static Time fromUnixSeconds(double value);
    static Time now();

    double secondsSinceUnixEpoch() const;
    std::string formatUTC(
        const char * format = "%A, %m/%d/%y %I:%M:%S %p") const;

    Time operator+(TimeSpan span) const;
    Time operator-(TimeSpan span) const;
    TimeSpan operator-(Time other) const;

private:
    explicit Time(double value);

    double seconds_ = 0.0;
};

} // namespace obol

#endif // OBOL_BASE_TIME_H
