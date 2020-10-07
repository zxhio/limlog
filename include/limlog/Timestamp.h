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

  /// Formatted string of today.
  /// e.g. 20180805
  std::string date() const { return std::string(datetime(), 0, 8); }

  /// Formatted string of current second include date and time.
  /// e.g. 20180805 14:45:20
  std::string datetime() const {
    // reduce count of calling strftime by thread_local.
    static thread_local time_t t_second = 0;
    static thread_local char t_datetime[24]; // 20190816 15:32:25
    time_t nowsec = timestamp_ / kUSecPerSec;
    if (t_second < nowsec) {
      t_second = nowsec;
      struct tm st_time;
      localtime_r(&t_second, &st_time);
      strftime(t_datetime, sizeof(t_datetime), "%Y%m%d %H:%M:%S", &st_time);
    }

    return t_datetime;
  }

  /// Formatted string of current second include time only.
  /// e.g. 144836
  std::string time() const {
    std::string time(datetime(), 9, 16);
    time.erase(std::remove(time.begin(), time.end(), ':'), time.end());
    return time;
  }

  /// Formatted string of current timestamp. eg, 20190701 16:28:30.070981
  /// e.g. 20200709 14:48:36.458074
  std::string formatTimestamp() const {
    char format[28];
    uint32_t micro = static_cast<uint32_t>(timestamp_ % kUSecPerSec);
    snprintf(format, sizeof(format), "%s.%06u", datetime().c_str(), micro);
    return format;
  }

  /// Current timestamp (us).
  /// e.g. 1594277460153980
  uint64_t timestamp() const { return timestamp_; }

private:
  uint64_t timestamp_;
  static const uint32_t kUSecPerSec = 1000000;
};

} // namespace detail
} // namespace limlog