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

using namespace limlog::detail;

void test_timestamp() {
#if 1
  TEST_STRING_EQ(Timestamp::now().format(), "2021-04-15-14:42:01.942083");
  TEST_INT_EQ(Timestamp::now().year(), 2021);
  TEST_INT_EQ(Timestamp::now().mon(), 4);
  TEST_INT_EQ(Timestamp::now().mday(), 15);
  TEST_INT_EQ(Timestamp::parse("2021-04-15-14:42:01.942083").year(), 2021);
  TEST_INT_EQ(Timestamp::parse("2021-04-15-14:42:01.942083").mon(), 4);
  TEST_INT_EQ(Timestamp::parse("2021-04-15-14:42:01.942083").mday(), 15);
  TEST_INT_EQ(Timestamp::parse("2021-04-15-14:42:01.942083").hour(), 14);
  TEST_INT_EQ(Timestamp::parse("2021-04-15-14:42:01.942083").min(), 42);
  TEST_INT_EQ(Timestamp::parse("2021-04-15-14:42:01.942083").sec(), 1);
  TEST_INT_EQ(Timestamp::parse(Timestamp::now().format()).sec(),
              Timestamp::now().sec());
  TEST_INT_EQ(
      Timestamp(Timestamp::parse(Timestamp::now().format()).timestamp()).year(),
      2021);
  TEST_INT_EQ(
      Timestamp(Timestamp::parse(Timestamp::now().format()).timestamp()).sec(),
      Timestamp::now().sec());
#endif
}

int main() {
  test_timestamp();
  PRINT_PASS_RATE();
  return !ALL_TEST_PASS();
}