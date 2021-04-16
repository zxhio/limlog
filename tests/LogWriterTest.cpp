//===- LogWriterTest.cpp - Log Writer Test ----------------------*- C++ -*-===//
//
/// \file
/// Log writer test routine
//
// Author:  zxh
// Date:    2021/04/12 11:06:25
//===----------------------------------------------------------------------===//

#include "Test.h"
#include <limlog/LogWriter.h>

#include <iostream>

using namespace limlog;

#define T_R(path, file, base, d, e)                                            \
  do {                                                                         \
    RotateWriter R(path, 0, 0, 0);                                             \
    std::string fullpath = R.filename();                                       \
    TEST_STRING_EQ(fullpath, file);                                            \
    TEST_STRING_EQ(basename(fullpath), base);                                  \
    TEST_STRING_EQ(dir(fullpath), d);                                          \
    TEST_STRING_EQ(ext(fullpath), e);                                          \
  } while (0)

static void test_rotate() {
  T_R("", "/tmp/lim.log", "lim.log", "/tmp", ".log");
  T_R("limlog", "limlog", "limlog", ".", "");
  T_R("lim.log", "lim.log", "lim.log", ".", ".log");
  T_R("../lim.log", "../lim.log", "lim.log", "..", ".log");
  T_R("./lim.log", "./lim.log", "lim.log", ".", ".log");
  T_R("tmp/lim.log", "tmp/lim.log", "lim.log", "tmp", ".log");
  T_R("/tmp/lim.log", "/tmp/lim.log", "lim.log", "/tmp", ".log");
  T_R("/tmp/lim.log.1", "/tmp/lim.log.1", "lim.log.1", "/tmp", ".1");
  T_R("./tmp/lim.log.1", "./tmp/lim.log.1", "lim.log.1", "./tmp", ".1");
  T_R("../tmp/lim.log.1", "../tmp/lim.log.1", "lim.log.1", "../tmp", ".1");
}

int main() {
  test_rotate();
  PRINT_PASS_RATE();
  return 0;
}