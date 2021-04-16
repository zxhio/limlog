//===- LogTest.cpp - Benchmark ----------------------------------*- C++ -*-===//
//
/// \file
/// Benchmark.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/12 11:41:58
//===----------------------------------------------------------------------===//

#include <limlog/Log.h>

using namespace limlog;
using namespace limlog::detail;

const int kLogTestCount = 1000000;
const int kTestThreadCount = 1;

uint64_t sys_clock_now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock().now().time_since_epoch())
      .count();
}

#define LOG_TIME(func, type, n, idx)                                           \
  do {                                                                         \
    uint64_t start = sys_clock_now();                                          \
    func();                                                                    \
    uint64_t end = sys_clock_now();                                            \
    fprintf(stdout,                                                            \
            "thread: %d, %d (%s) logs takes %" PRIu64                          \
            " us, average: %.2lf us\n",                                        \
            idx, kLogTestCount *n, type, end - start,                          \
            static_cast<double>(end - start) / kLogTestCount / n);             \
  } while (0)

void log_1_same_element_x6() {
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << i;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << 3.14159;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << true;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << 'c';

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << "c@string";

  std::string str = "std::string";
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << str;
}

void log_4_same_element_x6() {
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << i << i + 1 << i + 2 << i + 3;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << 3.14159 << 1.12312 << 1.01 << 1.1;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << true << false << true << false;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << 'c' << 'd' << 'e' << 'f';

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << "c@string"
              << "hello"
              << "world"
              << "the c program";

  std::string str = "std::string";
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << str << str << str << str;
}

void log_16_same_element_x6() {
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << i << i + 1 << i + 2 << i + 3 << i + 4 << i + 5 << i + 6
              << i + 7 << i + 8 << i + 9 << i + 10 << i + 11 << i + 12 << i + 13
              << i + 14 << i + 15;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << 3.14159 << 1.12312 << 1.01 << 1.1 << 3.14159 << 1.12312 << 1.01
              << 1.1 << 3.14159 << 1.12312 << 1.01 << 1.1 << 3.14159 << 1.12312
              << 1.01 << 1.1;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << true << false << true << false << true << false << true
              << false << true << false << true << false << true << false
              << true << false;

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << 'c' << 'd' << 'e' << 'f' << 'c' << 'd' << 'e' << 'f' << 'c'
              << 'd' << 'e' << 'f' << 'c' << 'd' << 'e' << 'f';

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << "c@string"
              << "hello"
              << "world"
              << "the c program"
              << "c@string"
              << "hello"
              << "world"
              << "the c program"
              << "c@string"
              << "hello"
              << "world"
              << "the c program"
              << "c@string"
              << "hello"
              << "world"
              << "the c program";

  std::string str("std::string");
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << str << str << str << str << str << str << str << str << str
              << str << str << str << str << str << str << str;
}

void log_10_diff_element_x1() {
  char ch = 'a';
  int16_t int16 = INT16_MIN;
  uint16_t uint16 = UINT16_MAX;
  int32_t int32 = INT32_MIN;
  uint32_t uint32 = UINT32_MAX;
  int64_t int64 = INT64_MIN;
  uint64_t uint64 = UINT64_MAX;
  double d = 1.844674;
  std::string str("std::string");

  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << ch << int16 << uint16 << int32 << uint32 << int64 << uint64
              << d << "c@string" << str;
}

void log_10_diff_element_len(const char *data, size_t n, const char *type,
                             int thread_idx) {
  char ch = 'a';
  int16_t int16 = INT16_MIN;
  uint16_t uint16 = UINT16_MAX;
  int32_t int32 = INT32_MIN;
  uint32_t uint32 = UINT32_MAX;
  int64_t int64 = INT64_MIN;
  uint64_t uint64 = UINT64_MAX;
  double d = 1.844674;
  std::string str("std::string");
  uint64_t start = sys_clock_now();
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << ch << int16 << uint16 << int32 << uint32 << int64 << uint64
              << d << "c@string" << str << data;

  uint64_t end = sys_clock_now();
  fprintf(stdout,
          "thread: %d, %d (%s) logs takes %" PRIu64 " us, average: %.2lf us\n",
          thread_idx, kLogTestCount, type, end - start,
          static_cast<double>(end - start) / kLogTestCount);
}

void log_10_diff_element_len(const std::string &s, const char *type,
                             int thread_idx) {
  char ch = 'a';
  int16_t int16 = INT16_MIN;
  uint16_t uint16 = UINT16_MAX;
  int32_t int32 = INT32_MIN;
  uint32_t uint32 = UINT32_MAX;
  int64_t int64 = INT64_MIN;
  uint64_t uint64 = UINT64_MAX;
  double d = 1.844674;
  std::string str("std::string");
  uint64_t start = sys_clock_now();
  for (int i = 0; i < kLogTestCount; ++i)
    LOG_DEBUG << ch << int16 << uint16 << int32 << uint32 << int64 << uint64
              << d << "c@string" << str << s;

  uint64_t end = sys_clock_now();
  fprintf(stdout,
          "thread: %d, %d (%s) logs takes %" PRIu64 " us, average: %.2lf us\n",
          thread_idx, kLogTestCount, type, end - start,
          static_cast<double>(end - start) / kLogTestCount);
}

void log_10_diff_element_str(int thread_idx) {
  std::string str(64, '1');
  log_10_diff_element_len(
      str.c_str(), 64, "10 diff element logs + 64 bytes c@string", thread_idx);
  log_10_diff_element_len(str, "10 diff element logs + 64 bytes std::string",
                          thread_idx);

  str = std::string(256, '2');
  log_10_diff_element_len(str.c_str(), 256,
                          "10 diff element logs + 256 bytes c@string",
                          thread_idx);
  log_10_diff_element_len(str, "10 diff element logs + 256 bytes std::string",
                          thread_idx);

  str = std::string(1024, '3');
  log_10_diff_element_len(str.c_str(), 1024,
                          "10 diff element logs + 1024 bytes c@string",
                          thread_idx);
  log_10_diff_element_len(str, "10 diff element logs + 1024 bytes std::string",
                          thread_idx);

  str = std::string(4096, '4');
  log_10_diff_element_len(str.c_str(), 4096,
                          "10 diff element logs + 4096 bytes c@string",
                          thread_idx);
  log_10_diff_element_len(str, "10 diff element logs + 4096 bytes std::string",
                          thread_idx);
}

void benchmark(int thread_idx) {
  LOG_TIME(log_1_same_element_x6, "1 same element logs x 6", 6, thread_idx);
  LOG_TIME(log_4_same_element_x6, "4 same element logs x 6", 6, thread_idx);
  LOG_TIME(log_16_same_element_x6, "16 same element logs x 6", 6, thread_idx);
  LOG_TIME(log_10_diff_element_x1, "10 diff element logs x 1", 1, thread_idx);
  log_10_diff_element_str(thread_idx);
}

int main() {

  setLogFile("./logs/test_log_file.log");
  setLogLevel(limlog::LogLevel::DEBUG);
  setWriter(limlog::make_unique<limlog::NullWriter>());
  // setWriter(limlog::make_unique<limlog::RotateWriter>("logs/test_log_file.log", 64, 5, 5));
  setMaxSize(256); // 64MB
  setMaxBackups(10);

  std::vector<std::thread> threads;
  for (int i = 0; i < kTestThreadCount; ++i)
    threads.emplace_back(std::thread(benchmark, i));

  for (int i = 0; i < kTestThreadCount; ++i)
    threads[i].join();

  return 0;
}
