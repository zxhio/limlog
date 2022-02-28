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

#include <limlog.h>

using namespace limlog;

#define TEST_STRING_INTEGER_EQ(actual, expect)                                 \
  do {                                                                         \
    char buf[32];                                                              \
    size_t len = formatInt(actual, buf);                                       \
    TEST_STRING_EQ(std::string(buf, len), expect);                             \
  } while (0)

void test_itoa() {
#if 1
  // test signed integer conversion.
  TEST_STRING_INTEGER_EQ(0, "0");
  TEST_STRING_INTEGER_EQ(-0, "0");
  TEST_STRING_INTEGER_EQ(-1, "-1");
  TEST_STRING_INTEGER_EQ(1, "1");
  TEST_STRING_INTEGER_EQ(INT8_MAX, "127");
  TEST_STRING_INTEGER_EQ(INT8_MIN, "-128");
  TEST_STRING_INTEGER_EQ(UINT8_MAX, "255");
  TEST_STRING_INTEGER_EQ(INT16_MAX, "32767");
  TEST_STRING_INTEGER_EQ(INT16_MIN, "-32768");
  TEST_STRING_INTEGER_EQ(UINT16_MAX, "65535");
  TEST_STRING_INTEGER_EQ(INT32_MAX, "2147483647");
  TEST_STRING_INTEGER_EQ(INT32_MIN, "-2147483648");
  TEST_STRING_INTEGER_EQ(UINT32_MAX, "4294967295");
  TEST_STRING_INTEGER_EQ(INT64_MAX, "9223372036854775807");
  TEST_STRING_INTEGER_EQ(INT64_MIN, "-9223372036854775808");
  TEST_STRING_INTEGER_EQ(UINT64_MAX, "18446744073709551615");
#endif
}

int main() {
  test_itoa();

  PRINT_PASS_RATE();

  return !ALL_TEST_PASS();
}