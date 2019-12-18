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

const uint32_t kTestCount = 10000;

void thr_1() {
    uint64_t start = util::Timestamp::now().timestamp();
    for (uint32_t i = 0; i < kTestCount; i++)
        LOG_ERROR << i << 'c' << "hello, world";
    uint64_t end = util::Timestamp::now().timestamp();

    printf("Thread 1 total times: %lu ms, avg time: %lu ns\n", end - start,
           (end - start) / (kTestCount / 1000));
}

void thr_2() {
    uint64_t start = util::Timestamp::now().timestamp();
    for (uint32_t i = 0; i < kTestCount; i++)
        LOG_ERROR << i << 'c' << "hello, world";
    uint64_t end = util::Timestamp::now().timestamp();

    printf("Thread 2 total times: %lu ms, avg time: %lu ns\n", end - start,
           (end - start) / (kTestCount / 1000));
}

void test_timestamp() {
    printf("data: %s\n", util::Timestamp::now().date().c_str());
    printf("datetime: %s\n", util::Timestamp::now().datetime().c_str());
    printf("time: %s\n", util::Timestamp::now().time().c_str());
    printf("format time: %s\n",
           util::Timestamp::now().formatTimestamp().c_str());
    printf("timestamp: %lu\n", util::Timestamp::now().timestamp());
}
int main() {
    // test_timestamp();

    setLogFile("./test_log_file");
    setLogLevel(limlog::LogLevel::DEBUG);
    setRollSize(64); // 64MB

    LOG_DEBUG << "我擦";

    std::thread t1(thr_1);
    std::thread t2(thr_2);

    t1.join();
    t2.join();
}