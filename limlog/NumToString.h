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
#include <stddef.h>

namespace util {

/// Convert unsigned integer \p n to string \p to , and return string length.
size_t u16toa(uint16_t n, char *to);
size_t u32toa(uint32_t n, char *to);
size_t u64toa(uint64_t n, char *to);

/// Convert signed integer \p n to string \p to , and return string length.
size_t i16toa(int16_t n, char *to);
size_t i32toa(int32_t n, char *to);
size_t i64toa(int64_t n, char *to);

} // namespace util
