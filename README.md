# limlog ![](https://www.travis-ci.org/zxhio/limlog.svg?branch=master)

Lightweight, easy-to-use and fast logging library, providing only a front-end for log writing.

Header-only, cross-platform which implemented in C++11. [细节分析](https://www.cnblogs.com/shuqin/p/12103952.html)

limlog uses the `singleton` pattern and is recommended for simple logging scenarios

## Usage

Header only, simply include 'limlog.h'.

```cpp
#include "limlog.h"

int main() {
  limlog::singleton()->setLogLevel(limlog::kDebug);

  LOG_INFO << 123 << ' ' << 1.23 << ' ' << true << ' ' << "123";
  return 0;
}
```

A logline format as follow:
```shell
//  +-------+------+-----------+------+------+------------+------+
//  | level | time | thread id | logs | file | (function) | line |
//  +-------+------+-----------+------+------+------------+------+

// such as:
INFO 2022-02-28T15:45:56.341+08:00 25332 fuck.cpp:4 123 1.230000 true 123
```

### Logging Output
limlog does not provide a rotation utility for logs, which is required for external programs.

But limlog provides an output interface for users to customize.
```cpp
#include "limlog.h"

// send log info to remote server.
ssize_t send_to_remote(const char *, size_t n) { 
  // do send
  return 0;
}

int main() {
  limlog::singleton()->setOutput(send_to_remote);

  LOG_INFO << 123 << ' ' << 1.23 << ' ' << true << ' ' << "123";
  return 0;
}
```

## Optimization

### Time
see https://github.com/zxhio/time_rfc3339.

### Thread local cache thread id
Introduce thread_local to avoid race conditions between threads. And reduce the number of gettid system calls

### Number to String
Uses search table to optimise integer and peer search  can confirm two characters.


### TODO
1. support more pattern for logging.
2. support async process.

### Reference
1. [Iyengar111/NanoLog](https://github.com/Iyengar111/NanoLog), Low Latency C++11 Logging Library.
2. [PlatformLab/NanoLog](https://github.com/PlatformLab/NanoLog), Nanolog is an extremely performant nanosecond scale logging system for C++ that exposes a simple printf-like API.
3. [kfifo](https://github.com/torvalds/linux/blob/master/lib/kfifo.c), kernel ring buffer.
5. [itoa-benchmark](https://github.com/miloyip/itoa-benchmark), some itoa algorithm, limlog uses search table.