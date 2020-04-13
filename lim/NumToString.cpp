//===- NumToString.cpp - Number to string -----------------------*- C++ -*-===//
//
/// \file
/// Faster conversion for number to string with look up digits table.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/29 17:48:39
//===----------------------------------------------------------------------===//

#include "NumToString.h"

#include <type_traits>

namespace util {

// The digits table is used to look up for number within 100.
// Each two character corresponds to one digit and ten digits.
static const char DigitsTable[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0',
    '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2',
    '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3',
    '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5',
    '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6',
    '7', '6', '8', '6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8', '0', '8', '1', '8',
    '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9',
    '7', '9', '8', '9', '9'};

static size_t u2a(uint64_t number, char *to) {
    char buf[24];
    char *p = buf;
    size_t length = 0;

    while (number >= 100) {
        const unsigned idx = (number % 100) << 1;
        number /= 100;
        *p++ = DigitsTable[idx + 1];
        *p++ = DigitsTable[idx];
    }

    if (number < 10) {
        *p++ = number + '0';
    } else {
        const unsigned idx = number << 1;
        *p++ = DigitsTable[idx + 1];
        *p++ = DigitsTable[idx];
    }

    length = p - buf;

    do {
        *to++ = *--p;
    } while (p != buf);
    *to = '\0';

    return length;
}

size_t i2a(int64_t number, char *to) {
    uint64_t n = static_cast<uint64_t>(number);
    size_t sign = 0;
    if (number < 0) {
        *to++ = '-';
        n = ~n + 1;
        sign = 1;
    }

    return u2a(n, to) + sign;
}

size_t u16toa(uint16_t n, char *to) { return u2a(n, to); }
size_t u32toa(uint32_t n, char *to) { return u2a(n, to); }
size_t u64toa(uint64_t n, char *to) { return u2a(n, to); }

size_t i16toa(int16_t n, char *to) { return i2a(n, to); }
size_t i32toa(int32_t n, char *to) { return i2a(n, to); }
size_t i64toa(int64_t n, char *to) { return i2a(n, to); }

} // namespace util
