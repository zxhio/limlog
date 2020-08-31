//===- ItoaTest.cpp - Itoa Test ----------------------------------------*- C++
//-*-===//
//
/// \file
/// Test of Digits to string.
//
// Author:  zxh
// Date:    2020/07/09 23:47:38
//===----------------------------------------------------------------------===//

#include "Test.h"

#include <limlog/NumToString.h>

using namespace limlog::detail;

#define TEST_STRING_INTEGER_EQ(actual, expect, func)                           \
  do {                                                                         \
    char tmp[24];                                                              \
    func(actual, tmp);                                                         \
    TEST_STRING_EQ(tmp, expect);                                               \
  } while (0)

#define TEST_SIGNED_EQ(actual, expect)                                         \
  do {                                                                         \
    TEST_STRING_INTEGER_EQ(actual, expect, i16toa);                            \
    TEST_STRING_INTEGER_EQ(actual, expect, i32toa);                            \
    TEST_STRING_INTEGER_EQ(actual, expect, i64toa);                            \
  } while (0)

#define TEST_UNSIGNED_EQ(actual, expect)                                       \
  do {                                                                         \
    TEST_STRING_INTEGER_EQ(actual, expect, u16toa);                            \
    TEST_STRING_INTEGER_EQ(actual, expect, u32toa);                            \
    TEST_STRING_INTEGER_EQ(actual, expect, u64toa);                            \
  } while (0)

void test_itoa() {
#if 1

  // test signed integer conversion.
  TEST_SIGNED_EQ(0, "0");
  TEST_SIGNED_EQ(-0, "0");
  TEST_SIGNED_EQ(INT8_MAX, "127");
  TEST_SIGNED_EQ(INT16_MAX, "32767");
  TEST_STRING_INTEGER_EQ(INT16_MIN, "-32768", i16toa);
  // TEST_STRING_INTEGER_EQ(UINT16_MAX, "65535", i16toa); // compile error
  TEST_STRING_INTEGER_EQ(UINT16_MAX, "65535", i32toa);
  TEST_STRING_INTEGER_EQ(INT32_MAX, "2147483647", i32toa);
  TEST_STRING_INTEGER_EQ(INT32_MIN, "-2147483648", i32toa);
  TEST_STRING_INTEGER_EQ(UINT32_MAX, "4294967295", i32toa); // false
  TEST_STRING_INTEGER_EQ(UINT32_MAX, "-1", i32toa);
  TEST_STRING_INTEGER_EQ(UINT32_MAX, "4294967295", i64toa);
  TEST_STRING_INTEGER_EQ(INT64_MAX, "9223372036854775807", i64toa);
  TEST_STRING_INTEGER_EQ(INT64_MIN, "-9223372036854775808", i64toa);
  TEST_STRING_INTEGER_EQ(UINT64_MAX, "18446744073709551615", i64toa); // false
  TEST_STRING_INTEGER_EQ(UINT64_MAX, "-1", i64toa);

  // test unsigned integer conversion.
  TEST_UNSIGNED_EQ(0, "0");
  TEST_UNSIGNED_EQ(UINT8_MAX, "255");
  TEST_UNSIGNED_EQ(UINT16_MAX, "65535");
  TEST_STRING_INTEGER_EQ(UINT32_MAX, "4294967295", u32toa);
  TEST_STRING_INTEGER_EQ(UINT64_MAX, "18446744073709551615", u64toa);

#endif
}

int main() {
  test_itoa();

  PRINT_PASS_RATE();

  return !ALL_TEST_PASS();
}