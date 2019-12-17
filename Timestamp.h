//===- Timestamp.h - Wrapper of Timestamp -----------------------*- C++ -*-===//
//
/// \file
///  Provide related helper function of timestamp.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/17 20:21:54
//===----------------------------------------------------------------------===//

#pragma once

#include <string>

#include <stdint.h>

namespace util {

/// Wrapper of timestamp.
class Timestamp {
  public:
    Timestamp() : timestamp_(0) {}
    Timestamp(uint64_t timestamp) : timestamp_(timestamp) {}

    /// Timestamp of current timestamp.
    static Timestamp now();

    /// Formatted string of today. eg, 20180805
    std::string date() const;

    /// Formatted string of current second include date and time.
    /// eg, 20180805 14:45:20
    std::string datetime() const;

    /// Formatted string of current second include time only.
    std::string time() const;

    /// Formatted string of current timestamp. eg, 20190701 16:28:30.070981
    std::string formatTimestamp() const;

    /// Current timestamp.
    uint64_t timestamp() const { return timestamp_; }

  private:
    uint64_t timestamp_;
    static const uint32_t kMicrosecondOneSecond = 1000000;
};

} // namespace util
