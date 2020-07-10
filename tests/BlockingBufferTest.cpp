//===- BlockingBufferTest.cpp - BlockingBuffer Test -------------*- C++ -*-===//
//
/// \file
/// BlockingBuffer Test routine.
//
// Author:  zxh
// Date:    2020/07/09 23:21:31
//===----------------------------------------------------------------------===//

#include "Test.h"

#include <limlog/Log.h>

#include <assert.h>
#include <stdint.h>

using namespace limlog;

#define TEST_BUFFER(buf, s, u, unu, c)                                         \
  do {                                                                         \
    TEST_INT_EQ(buf->size(), s);                                               \
    TEST_INT_EQ(buf->used(), u);                                               \
    TEST_INT_EQ(buf->unused(), unu);                                           \
    TEST_INT_EQ(buf->consumable(), c);                                         \
  } while (0)

#define TEST_BUFFER_PRODUCE(buf, from, n, s, u, unu, c)                        \
  do {                                                                         \
    buf->produce(from, n);                                                     \
    TEST_BUFFER(buf, s, u, unu, c);                                            \
  } while (0)

#define TEST_BUFFER_CONSUMABLE(buf, n, s, u, unu, c)                           \
  do {                                                                         \
    buf->incConsumablePos(n);                                                  \
    TEST_BUFFER(buf, s, u, unu, c);                                            \
  } while (0)

#define TEST_BUFFER_CONSUME(buf, to, n, s, u, unu, c)                          \
  do {                                                                         \
    buf->consume(to, n);                                                       \
    TEST_BUFFER(buf, s, u, unu, c);                                            \
  } while (0)

void test_blocking_buffer() {

#if 1

  // alloc memory before test.
  const uint32_t kBytesPerMb = 1 << 20;
  const uint32_t kBytesPerKb = 1 << 10;

  char ch = 'c'; // 1 character
  char *mem_128b = static_cast<char *>(malloc(sizeof(char) * 128));
  char *mem_1kb = static_cast<char *>(malloc(sizeof(char) * kBytesPerKb));
  char *mem_64kb = static_cast<char *>(malloc(sizeof(char) * kBytesPerKb * 64));
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
  TEST_BUFFER_PRODUCE(buf, mem_64kb, kBytesPerKb * 64, size, used, size - used,
                      0);
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
  TEST_BUFFER_CONSUME(buf, mem_128b, 128, size, used, size - used, kBytesPerKb);
  used -= kBytesPerKb;
  TEST_BUFFER_CONSUME(buf, mem_1kb, kBytesPerKb, size, used, size - used, 0);
  TEST_BUFFER_CONSUMABLE(buf, 320 * kBytesPerKb + 719743, size, used,
                         size - used, 320 * kBytesPerKb + 719743);
  used -= kBytesPerKb * 64;
  TEST_BUFFER_CONSUME(buf, mem_64kb, kBytesPerKb * 64, size, used, size - used,
                      256 * kBytesPerKb + 719743);
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

int main() {
  test_blocking_buffer();

  PRINT_PASS_RATE();

  return !ALL_TEST_PASS();
}
