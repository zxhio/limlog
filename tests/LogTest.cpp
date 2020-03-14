//===- LogTest.cpp - Test Routine -------------------------------*- C++ -*-===//
//
/// \file
/// Logging Test routine.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/12 11:41:58
//===----------------------------------------------------------------------===//

#include "../lim/Log.h"
#include "../lim/Timestamp.h"
#include "../lim/NumToString.h"

#include <assert.h>
#include <stdio.h>

using namespace limlog;
using namespace util;

const int kLogTestCount = 100000;
const int kTestThreadCount = 2;
int test_count = 0;
int test_pass = 0;

#define EQ(equality, actual, expect, format)                                   \
    do {                                                                       \
        test_count++;                                                          \
        if (equality)                                                          \
            test_pass++;                                                       \
        else {                                                                 \
            fprintf(stderr, "%s:%d: actual: " format " expect: " format "\n",  \
                    __FILE__, __LINE__, actual, expect);                       \
        }                                                                      \
    } while (0)

#define TEST_INT_EQ(actual, expect) EQ(actual == expect, actual, expect, "%d")
#define TEST_CHAR_EQ(actual, expect) EQ(actual == expect, actual, expect, "%c")

#define TEST_STRING_EQ(actual, expect)                                         \
    do {                                                                       \
        std::string a(actual);                                                 \
        std::string e(expect);                                                 \
        EQ(a.compare(e) == 0, a.c_str(), e.c_str(), "%s");                     \
    } while (0)

void test_timestamp() {
#if 1
    TEST_STRING_EQ(Timestamp::now().date(), "20191230");
#endif
}

#define TEST_BUFFER(buf, s, u, unu, c)                                         \
    do {                                                                       \
        TEST_INT_EQ(buf->size(), s);                                           \
        TEST_INT_EQ(buf->used(), u);                                           \
        TEST_INT_EQ(buf->unused(), unu);                                       \
        TEST_INT_EQ(buf->consumable(), c);                                     \
    } while (0)

#define TEST_BUFFER_PRODUCE(buf, from, n, s, u, unu, c)                        \
    do {                                                                       \
        buf->produce(from, n);                                                 \
        TEST_BUFFER(buf, s, u, unu, c);                                        \
    } while (0)

#define TEST_BUFFER_CONSUMABLE(buf, n, s, u, unu, c)                           \
    do {                                                                       \
        buf->incConsumablePos(n);                                              \
        TEST_BUFFER(buf, s, u, unu, c);                                        \
    } while (0)

#define TEST_BUFFER_CONSUME(buf, to, n, s, u, unu, c)                          \
    do {                                                                       \
        buf->consume(to, n);                                                   \
        TEST_BUFFER(buf, s, u, unu, c);                                        \
    } while (0)

void test_blocking_buffer() {
#if 1

    // alloc memory before test.
    const uint32_t kBytesPerMb = 1 << 20;
    const uint32_t kBytesPerKb = 1 << 10;

    char ch = 'c'; // 1 character
    char *mem_128b = static_cast<char *>(malloc(sizeof(char) * 128));
    char *mem_1kb = static_cast<char *>(malloc(sizeof(char) * kBytesPerKb));
    char *mem_64kb =
        static_cast<char *>(malloc(sizeof(char) * kBytesPerKb * 64));
    char *mem_256kb =
        static_cast<char *>(malloc(sizeof(char) * kBytesPerKb * 256));
    char *mem_1mb = static_cast<char *>(malloc(sizeof(char) * kBytesPerMb));
    memset(mem_128b, '1', 128);
    memset(mem_1kb, '2', kBytesPerKb);
    memset(mem_64kb, '3', kBytesPerKb * 64);
    memset(mem_256kb, '4', kBytesPerKb * 256);
    memset(mem_1mb, '5', kBytesPerMb);

    BlockingBuffer *buf =
        static_cast<BlockingBuffer *>(malloc(sizeof(BlockingBuffer)));
    uint32_t size = buf->size();
    uint32_t used = 0;
    assert(size == kBytesPerMb);
    TEST_BUFFER(buf, kBytesPerMb, 0, kBytesPerMb, 0);

    // test BlockingBuffer::produce()
    TEST_BUFFER_PRODUCE(buf, &ch, 0, size, 0, size, 0); // 0 bytes
    used += 1;
    TEST_BUFFER_PRODUCE(buf, &ch, 1, size, 1, size - 1, 0);
    used += 128;
    TEST_BUFFER_PRODUCE(buf, mem_128b, 128, size, used, size - used, 0);
    used += kBytesPerKb;
    TEST_BUFFER_PRODUCE(buf, mem_1kb, kBytesPerKb, size, used, size - used, 0);
    used += kBytesPerKb * 64;
    TEST_BUFFER_PRODUCE(buf, mem_64kb, kBytesPerKb * 64, size, used,
                        size - used, 0);
    used += kBytesPerKb * 256;
    TEST_BUFFER_PRODUCE(buf, mem_256kb, kBytesPerKb * 256, size, used,
                        size - used, 0);
    // the rest of BlockingBuffer is 719743 bytes.
    // 719743 = size(1Mb) - 1b - 128b - 1kb - 64kb - 256kb
    // now. the max bytes of producable is 719743.
    used += 719743;
    TEST_BUFFER_PRODUCE(buf, mem_1mb, 719743, size, used, size - used, 0);

#if 0
    // blocking buffer is full, it will be blocking if we produce a character.
    used += 1;
    TEST_BUFFER_PRODUCE(buf, mem_1mb, 1, size, used, size - used, 0);
#endif

    // test BlockingBuffer::consume() and BlockingBuffer::consumbale().
    TEST_BUFFER_CONSUMABLE(buf, 0, size, used, size - used, 0);
    TEST_BUFFER_CONSUMABLE(buf, 1, size, used, size - used, 1);
    TEST_BUFFER_CONSUME(buf, &ch, 0, size, used, size - used, 1);
    used -= 1;
    TEST_BUFFER_CONSUME(buf, &ch, 1, size, used, size - used, 0);

    TEST_BUFFER_CONSUMABLE(buf, 128, size, used, size - used, 128);
    TEST_BUFFER_CONSUMABLE(buf, kBytesPerKb, size, used, size - used,
                           128 + kBytesPerKb);
    used -= 128;
    TEST_BUFFER_CONSUME(buf, mem_128b, 128, size, used, size - used,
                        kBytesPerKb);
    used -= kBytesPerKb;
    TEST_BUFFER_CONSUME(buf, mem_1kb, kBytesPerKb, size, used, size - used, 0);
    TEST_BUFFER_CONSUMABLE(buf, 320 * kBytesPerKb + 719743, size, used,
                           size - used, 320 * kBytesPerKb + 719743);
    used -= kBytesPerKb * 64;
    TEST_BUFFER_CONSUME(buf, mem_64kb, kBytesPerKb * 64, size, used,
                        size - used, 256 * kBytesPerKb + 719743);
    used -= kBytesPerKb * 256;
    TEST_BUFFER_CONSUME(buf, mem_256kb, kBytesPerKb * 256, size, used,
                        size - used, 719743);
    used -= 719743;
    TEST_BUFFER_CONSUME(buf, mem_1mb, 719743, size, 0, size, 0);

    // test circle buffer.
    TEST_BUFFER(buf, kBytesPerMb, 0, size, 0);
    TEST_BUFFER_PRODUCE(buf, mem_1mb, kBytesPerMb, size, kBytesPerMb, 0, 0);
    TEST_BUFFER_CONSUMABLE(buf, kBytesPerMb, size, kBytesPerMb, 0, kBytesPerMb);
    TEST_BUFFER_CONSUME(buf, mem_1mb, kBytesPerMb, size, 0, kBytesPerMb, 0);

    free(buf);
    free(mem_128b);
    free(mem_1kb);
    free(mem_64kb);
    free(mem_256kb);
    free(mem_1mb);
#endif
}

#define TEST_STRING_INTEGER_EQ(actual, expect, func)                           \
    do {                                                                       \
        char tmp[24];                                                          \
        func(actual, tmp);                                                     \
        TEST_STRING_EQ(tmp, expect);                                           \
    } while (0)

#define TEST_SIGNED_EQ(actual, expect)                                         \
    do {                                                                       \
        TEST_STRING_INTEGER_EQ(actual, expect, i16toa);                        \
        TEST_STRING_INTEGER_EQ(actual, expect, i32toa);                        \
        TEST_STRING_INTEGER_EQ(actual, expect, i64toa);                        \
    } while (0)

#define TEST_UNSIGNED_EQ(actual, expect)                                       \
    do {                                                                       \
        TEST_STRING_INTEGER_EQ(actual, expect, u16toa);                        \
        TEST_STRING_INTEGER_EQ(actual, expect, u32toa);                        \
        TEST_STRING_INTEGER_EQ(actual, expect, u64toa);                        \
    } while (0)

void test_itoa() {
#if 1
    // test signed integer conversion.
    TEST_SIGNED_EQ(0, "0");
    TEST_SIGNED_EQ(-0, "0");
    TEST_SIGNED_EQ(INT8_MAX, "127");
    TEST_SIGNED_EQ(INT16_MAX, "32767");
    TEST_STRING_INTEGER_EQ(INT16_MIN, "-32768", i16toa);
    // TEST_STRING_INTEGER_EQ(UINT16_MAX, "65535", i16toa); // false
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

#define LOG_TIME(func, type, n, idx)                                           \
    do {                                                                       \
        uint64_t start = Timestamp::now().timestamp();                         \
        func();                                                                \
        uint64_t end = Timestamp::now().timestamp();                           \
        fprintf(stdout,                                                        \
                "thread: %d, %d (%s) logs takes %ju us, average: %.2lf us\n",  \
                idx, kLogTestCount *n, type, end - start,                      \
                static_cast<double>(end - start) / kLogTestCount / n);         \
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
                  << i + 7 << i + 8 << i + 9 << i + 10 << i + 11 << i + 12
                  << i + 13 << i + 14 << i + 15;

    for (int i = 0; i < kLogTestCount; ++i)
        LOG_DEBUG << 3.14159 << 1.12312 << 1.01 << 1.1 << 3.14159 << 1.12312
                  << 1.01 << 1.1 << 3.14159 << 1.12312 << 1.01 << 1.1 << 3.14159
                  << 1.12312 << 1.01 << 1.1;

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
    uint64_t start = Timestamp::now().timestamp();
    for (int i = 0; i < kLogTestCount; ++i)
        LOG_DEBUG << ch << int16 << uint16 << int32 << uint32 << int64 << uint64
                  << d << "c@string" << str << data;

    uint64_t end = Timestamp::now().timestamp();
    fprintf(stdout,
            "thread: %d, %d (%s) logs takes %ju us, average: %.2lf us\n",
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
    uint64_t start = Timestamp::now().timestamp();
    for (int i = 0; i < kLogTestCount; ++i)
        LOG_DEBUG << ch << int16 << uint16 << int32 << uint32 << int64 << uint64
                  << d << "c@string" << str << s;

    uint64_t end = Timestamp::now().timestamp();
    fprintf(stdout,
            "thread: %d, %d (%s) logs takes %ju us, average: %.2lf us\n",
            thread_idx, kLogTestCount, type, end - start,
            static_cast<double>(end - start) / kLogTestCount);
}

void log_10_diff_element_str(int thread_idx) {
    std::string str(64, '1');
    log_10_diff_element_len(str.c_str(), 64,
                            "10 diff element logs + 64 bytes c@string",
                            thread_idx);
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
    log_10_diff_element_len(
        str, "10 diff element logs + 1024 bytes std::string", thread_idx);

    str = std::string(4096, '4');
    log_10_diff_element_len(str.c_str(), 4096,
                            "10 diff element logs + 4096 bytes c@string",
                            thread_idx);
    log_10_diff_element_len(
        str, "10 diff element logs + 4096 bytes std::string", thread_idx);
}

void benchmark(int thread_idx) {
    LOG_TIME(log_1_same_element_x6, "1 same element logs x 6", 6, thread_idx);
    LOG_TIME(log_4_same_element_x6, "4 same element logs x 6", 6, thread_idx);
    LOG_TIME(log_16_same_element_x6, "16 same element logs x 6", 6, thread_idx);
    LOG_TIME(log_10_diff_element_x1, "10 diff element logs x 1", 1, thread_idx);
    log_10_diff_element_str(thread_idx);
}

int main() {
    test_timestamp();
    test_blocking_buffer();
    test_itoa();

    fprintf(stderr, "[%.2f%%] all test: %d, pass: %d.\n",
            static_cast<float>(test_pass * 100) / test_count, test_count,
            test_pass);

    setLogFile("./test_log_file");
    setLogLevel(limlog::LogLevel::DEBUG);
    setRollSize(64); // 64MB

    std::vector<std::thread> threads;
    for (int i = 0; i < kTestThreadCount; ++i)
        threads.emplace_back(std::thread(benchmark, i));

    for (int i = 0; i < kTestThreadCount; ++i)
        threads[i].join();
}