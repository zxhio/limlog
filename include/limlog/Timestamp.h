//===- Timestamp.h - Wrapper of Timestamp -----------------------*- C++ -*-===//
//
/// \file
///  Provide related helper function of timestamp.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/17 20:21:54
//===----------------------------------------------------------------------===//

#pragma once

#include <algorithm>
#include <iostream>
#include <string>

#ifdef __linux
#include <sys/time.h>
#else
#include <chrono>
#endif

#include <stdint.h>

namespace limlog {
namespace detail {

/// Wrapper of timestamp.
class Timestamp {
public:
  Timestamp() : timestamp_(0) {}
  Timestamp(uint64_t timestamp) : timestamp_(timestamp) {}

  /// Timestamp of current timestamp.
  static Timestamp now() {
    uint64_t timestamp = 0;

#ifdef __linux
    // use gettimeofday(2) is 15% faster then std::chrono in linux.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timestamp = tv.tv_sec * kUSecPerSec + tv.tv_usec;
#else
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock().now().time_since_epoch())
                    .count();
#endif

    return Timestamp(timestamp);
  }

  /// Parse format '2020-07-09-14:48:36.458074' to Timestamp.
  static Timestamp parse(const std::string &fmt) {
    if (fmt.length() == 0)
      return Timestamp();

    struct tm st_tm;

    st_tm.tm_year = std::stoi(fmt.substr(0, 4)) - 1900;
    st_tm.tm_mon = std::stoi(fmt.substr(5, 6)) - 1;
    st_tm.tm_mday = std::stoi(fmt.substr(8, 9));
    st_tm.tm_hour = std::stoi(fmt.substr(11, 12));
    st_tm.tm_min = std::stoi(fmt.substr(14, 15));
    st_tm.tm_sec = std::stoi(fmt.substr(17, 18));
    uint32_t usec = std::stoi(fmt.substr(20, 25));

    time_t t = static_cast<uint64_t>(mktime(&st_tm) * kUSecPerSec + usec);

    return Timestamp(t);
  }

  /// Current timestamp (us).
  /// e.g. 1594277460153980
  uint64_t timestamp() const { return timestamp_; }

  int year() const { return toTm().tm_year + 1900; }
  int mon() const { return toTm().tm_mon + 1; }
  int mday() const { return toTm().tm_mday; }
  int hour() const { return toTm().tm_hour; }
  int min() const { return toTm().tm_min; }
  int sec() const { return toTm().tm_sec; }

  /// Format time with default fmt.
  /// e.g. 2020-07-09-14:48:36.458074
  std::string format() const;

  /// Compare Timestamp.
  int64_t compare(const Timestamp &t) const {
    return static_cast<int64_t>(timestamp_ - t.timestamp());
  }

private:
  /// Convert to 'struct tm'
  struct tm toTm() const;

  uint64_t timestamp_;
  static const uint32_t kUSecPerSec = 1000000;
};

std::string Timestamp::format() const {
  // reduce count of calling strftime by thread_local.
  static thread_local time_t t_second = 0;
  static thread_local char t_datetime[32]; // 2019-08-16-15:32:25
  time_t nowsec = timestamp_ / kUSecPerSec;
  if (t_second != nowsec) {
    t_second = nowsec;
    struct tm st_time;
    localtime_r(&t_second, &st_time);
    strftime(t_datetime, sizeof(t_datetime), "%Y-%m-%d-%H:%M:%S", &st_time);
  }

  char f[40];
  uint32_t micro = static_cast<uint32_t>(timestamp_ % kUSecPerSec);
  snprintf(f, sizeof(f), "%s.%06u", t_datetime, micro);
  return f;
}

struct tm Timestamp::toTm() const {
  time_t nowsec = timestamp_ / kUSecPerSec;
  struct tm st_time;
  localtime_r(&nowsec, &st_time);
  return st_time;
}

} // namespace detail
} // namespace limlog