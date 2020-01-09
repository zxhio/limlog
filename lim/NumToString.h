//===- NumToString.h - Number to string -------------------------*- C++ -*-===//
//
/// \file
/// Faster conversion for number to string.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/29 17:45:31
//===----------------------------------------------------------------------===//

#pragma once

#include <stdint.h>

namespace util {

/// Conversion unsigned integer to string \p to , and return string length.
int u16toa(uint16_t number, char *to);
int u32toa(uint32_t number, char *to);
int u64toa(uint64_t number, char *to);

/// Conversion signed integer to string \p to , and return string length.
int i16toa(int16_t number, char *to);
int i32toa(int32_t number, char *to);
int i64toa(int64_t number, char *to);

} // namespace util
