//===- LogTest.cpp - Test Routine -------------------------------*- C++ -*-===//
//
/// \file
/// Logging Test routine.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/12 11:41:58
//===----------------------------------------------------------------------===//

#include "Log.h"
#include "Timestamp.h"

using namespace limlog;

void thr_1() {
    for (int i = 0; i < 10000000; i++)
        produceLog("11111\n", 6);
}

void thr_2() {
    for (int i = 0; i < 10000000; i++)
        produceLog("22222\n", 6);
}

void test_timestamp() {
    printf("data: %s\n", util::Timestamp::now().date().c_str());
    printf("datetime: %s\n", util::Timestamp::now().datetime().c_str());
    printf("time: %s\n", util::Timestamp::now().time().c_str());
    printf("format time: %s\n",
           util::Timestamp::now().formatTimestamp().c_str());
    printf("timestamp: %llu\n", util::Timestamp::now().timestamp());
}
int main() {
    test_timestamp();

    setLogFile("./test_log_file");

    std::thread t1(thr_1);
    std::thread t2(thr_2);

    t1.join();
    t2.join();
}