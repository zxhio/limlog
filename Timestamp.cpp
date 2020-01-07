//===- Timestamp.cpp - Timestamp of Wrapper Implementation ------*- C++ -*-===//
//
//  Process timestamp to get the relevent string or number.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/17 20:35:14
//===----------------------------------------------------------------------===//

#include "Timestamp.h"

#include <chrono>
#include <algorithm>

#ifdef __linux
#include <sys/time.h>
#endif

#include <stdio.h>

namespace util {

thread_local time_t t_second = 0;
thread_local char t_datetime[24]; // 20190816 15:32:25

Timestamp Timestamp::now() {
    uint64_t timestamp = 0;

#ifdef __linux
    // use gettimeofday(2) is 15% faster then std::chrono in linux.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timestamp = tv.tv_sec * kMicrosecondOneSecond + tv.tv_usec;
#else
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock().now().time_since_epoch())
                    .count();
#endif

    return Timestamp(timestamp);
}

std::string Timestamp::datetime() const {
    time_t nowsec = timestamp_ / kMicrosecondOneSecond;
    if (t_second < nowsec) {
        t_second = nowsec;
        struct tm *st_time = localtime(&t_second);
        strftime(t_datetime, sizeof(t_datetime), "%Y%m%d %H:%M:%S", st_time);
    }

    return t_datetime;
}

std::string Timestamp::time() const {
    std::string time(datetime(), 9, 16);
    time.erase(std::remove(time.begin(), time.end(), ':'), time.end());
    return time;
}

std::string Timestamp::date() const { return std::string(datetime(), 0, 8); }

std::string Timestamp::formatTimestamp() const {
    char format[28];
    unsigned int microseconds =
        static_cast<unsigned int>(timestamp_ % kMicrosecondOneSecond);
    snprintf(format, sizeof(format), "%s.%06u", datetime().c_str(),
             microseconds);
    return format;
}

} // namespace util
