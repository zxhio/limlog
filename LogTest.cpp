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
    produceLog("thr_1", 5);
    produceLog("thread_1", 8);
}

void thr_2() {
    for (int i = 0; i < 10000000; i++)
        produceLog("thr_2", 5);
    produceLog("thread_2", 8);
}

int main() {
    setLogFile("./testLogFile");
    produceLog("hello, world", 12);
    produceLog("zengxianhui is wang ba dang.", 25);

    std::thread t1(thr_1);
    std::thread t2(thr_2);

    t1.join();
    t2.join();
}