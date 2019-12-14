//===- LogTest.cpp - Test Routine -------------------------------*- C++ -*-===//
//
/// \file
/// Logging Test routine.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/12 11:41:58
//===----------------------------------------------------------------------===//

#include "Log.h"

using namespace limlog;

void thr_1() {
    for (int i = 0; i < 10000000; i++)
        produceLog("11111\n", 6);
}

void thr_2() {
    for (int i = 0; i < 10000000; i++)
        produceLog("22222\n", 6);
}

int main() {
    setLogFile("./test_log_file.log");

    std::thread t1(thr_1);
    std::thread t2(thr_2);

    t1.join();
    t2.join();
}