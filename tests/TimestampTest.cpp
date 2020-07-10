//===- TimestampTest.cpp - Timestamp Test -----------------------*- C++ -*-===//
//
/// \file
/// Test routine of Timestamp.
//
// Author:  zxh
// Date:    2020/07/09 23:36:58
//===----------------------------------------------------------------------===//

#include "Test.h"

#include <limlog/Timestamp.h>

using namespace limlog::util;

void test_timestamp() {
#if 1
  TEST_STRING_EQ(Timestamp::now().date(), "20191230");
#endif
}

int main() {
  PRINT_PASS_RATE();
  return !ALL_TEST_PASS();
}