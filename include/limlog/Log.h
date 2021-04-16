//===- Log.h - Logging Interface --------------------------------*- C++ -*-===//
//
/// \file
/// A simple and fast logger with asynchronization.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/07 14:13:09
//===----------------------------------------------------------------------===//

#pragma once

#include "LogWriter.h"
#include "NumToString.h"
#include "Timestamp.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <inttypes.h>
#include <stdint.h> // uint32_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux
#include <sys/syscall.h> // gettid().
#include <unistd.h>
typedef pid_t thread_id_t;
#elif __APPLE__
#include <pthread.h>
typedef uint64_t thread_id_t;
#else
#include <sstream>
typedef unsigned int thread_id_t; // MSVC
#endif

namespace limlog {

inline thread_id_t gettid() {
  static thread_local thread_id_t t_tid = 0;

  if (t_tid == 0) {
#ifdef __linux
    t_tid = syscall(__NR_gettid);
#elif __APPLE__
    pthread_threadid_np(NULL, &t_tid);
#else
    std::stringstream ss;
    ss << std::this_thread::get_id();
    ss >> t_tid;
#endif
  }
  return t_tid;
}

/// Log level >= set level will be written to file.
enum LogLevel : uint8_t { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

// Stringify log level with width of 5.
static const char *stringifyLogLevel(LogLevel level) {
  switch (level) {
  case LogLevel::FATAL:
    return "FATAL";
  case LogLevel::ERROR:
    return "ERROR";
  case LogLevel::WARN:
    return "WARN ";
  case LogLevel::INFO:
    return "INFO ";
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::TRACE:
    return "TRACE";
  default:
    return "NONE ";
  }
}

/// Circle FIFO blocking produce/consume byte queue. Hold log info to wait for
/// background thread consume. It exists in each thread.
class BlockingBuffer {
public:
  BlockingBuffer()
      : producePos_(0), consumePos_(0), consumablePos_(0), produceCount_(0) {}

  /// Get position offset calculated from buffer start.
  uint32_t offsetOfPos(uint32_t pos) const { return pos & (size() - 1); }

  /// Buffer size.
  uint32_t size() const { return kBlockingBufferSize; }

  /// Already used bytes.
  /// It may be called by different threads, so add memory barrier to
  /// ensure the lasted *Pos_ is read.
  uint32_t used() const {
    std::atomic_thread_fence(std::memory_order_acquire);
    return producePos_ - consumePos_;
  }

  /// Unused bytes.
  uint32_t unused() const { return kBlockingBufferSize - used(); }

  /// Reset buffer's position.
  void reset() { producePos_ = consumePos_ = 0; }

  /// The position at the end of the last complete log.
  uint32_t consumable() const {
    std::atomic_thread_fence(std::memory_order_acquire);
    return consumablePos_ - consumePos_;
  }

  /// Increase consumable position with a complete log length \p n .
  void incConsumablePos(uint32_t n) {
    consumablePos_ += n;
    std::atomic_thread_fence(std::memory_order_release);
  }

  /// Peek the produce position in buffer.
  char *peek() { return &storage_[producePos_]; }

  /// consume n bytes data and only move the consume position.
  void consume(uint32_t n) { consumePos_ += n; }

  /// consume \p n bytes data to \p to .
  uint32_t consume(char *to, uint32_t n) {
    // available bytes to consume.
    uint32_t avail = std::min(consumable(), n);

    // offset of consumePos to buffer end.
    uint32_t off2End = std::min(avail, size() - offsetOfPos(consumePos_));

    // first put the data starting from consumePos until the end of buffer.
    memcpy(to, storage_ + offsetOfPos(consumePos_), off2End);

    // then put the rest at beginning of the buffer.
    memcpy(to + off2End, storage_, avail - off2End);

    consumePos_ += avail;
    std::atomic_thread_fence(std::memory_order_release);

    return avail;
  }

  /// Copy \p n bytes log info from \p from to buffer. It will be blocking
  /// when buffer space is insufficient.
  void produce(const char *from, uint32_t n) {
    while (unused() < n)
      /* blocking */;

    // offset of producePos to buffer end.
    uint32_t off2End = std::min(n, size() - offsetOfPos(producePos_));

    // first put the data starting from producePos until the end of buffer.
    memcpy(storage_ + offsetOfPos(producePos_), from, off2End);

    // then put the rest at beginning of the buffer.
    memcpy(storage_, from + off2End, n - off2End);

    produceCount_++;
    producePos_ += n;
    std::atomic_thread_fence(std::memory_order_release);
  }

private:
  static const uint32_t kBlockingBufferSize = 1 << 20; // 1 MB
  uint32_t producePos_;
  uint32_t consumePos_;
  uint32_t consumablePos_; // increase every time with a complete log length.
  uint32_t produceCount_;
  char storage_[kBlockingBufferSize]; // buffer size power of 2.
};

template <class T, class... Args>
std::unique_ptr<T> make_unique(Args &&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

/// Logging system.
/// Manage the action and attibute of logging.
class LimLog {
public:
  /// Log setting.
  LogLevel getLogLevel() const { return level_; };
  void setLogLevel(LogLevel level) { level_ = level; }
  void setLogFile(const char *file) { writer_->setFileName(file); };
  void setMaxSize(size_t nMB) { writer_->setMaxSize(nMB); };
  void setMaxBackups(size_t count) { writer_->setMaxBackups(count); }
  void setWriter(std::unique_ptr<Writer> w) { writer_.swap(w); }

  /// Singleton pointer.
  static LimLog *singleton() {
    static LimLog s_limlog;
    return &s_limlog;
  }

  /// Produce log data \p data to BlockingBuffer in each thread.
  void produce(const char *data, size_t n) {
    blockingBuffer()->produce(data, static_cast<uint32_t>(n));
  }

  /// Increase BlockingBuffer consumable position.
  void incConsumable(uint32_t n) {
    blockingBuffer()->incConsumablePos(n);
    logCount_.fetch_add(1, std::memory_order_relaxed);
  }

  /// List log related statistic.
  void listStatistic() const {
    printf("\n");
    printf("=== Logging System Related Data ===\n");
    printf("  Total produced logs count: [%" PRIu64 "].\n", logCount_.load());
    printf("  Total bytes of sinking to file: [%" PRIu64 "] bytes.\n",
           totalConsumeBytes_);
    printf("  Average bytes of sinking to file: [%" PRIu64 "] bytes.\n",
           sinkCount_ == 0 ? 0 : totalConsumeBytes_ / sinkCount_);
    printf("  Count of sinking to file: [%u].\n", sinkCount_);
    printf("  Total microseconds takes of sinking to file: [%" PRIu64 "] us.\n",
           totalSinkTimes_);
    printf("  Average microseconds takes of sinking to file: [%" PRIu64 "] us."
           "\n",
           sinkCount_ == 0 ? 0 : totalSinkTimes_ / sinkCount_);
    printf("\n");
  }

private:
  LimLog(const LimLog &) = delete;
  LimLog &operator=(const LimLog &) = delete;

  LimLog()
      : writer_(make_unique<StdoutWriter>()),
        threadSync_(false),
        threadExit_(false),
        outputFull_(false),
        level_(LogLevel::WARN),
        sinkCount_(0),
        logCount_(0),
        totalSinkTimes_(0),
        totalConsumeBytes_(0),
        perConsumeBytes_(0),
        bufferSize_(1 << 24),
        outputBuffer_(nullptr),
        doubleBuffer_(nullptr),
        threadBuffers_(),
        sinkThread_(),
        bufferMutex_(),
        condMutex_(),
        proceedCond_(),
        hitEmptyCond_() {
    outputBuffer_ = static_cast<char *>(malloc(bufferSize_));
    doubleBuffer_ = static_cast<char *>(malloc(bufferSize_));
    // alloc memory exception handle.

    sinkThread_ = std::thread(&LimLog::sinkThreadFunc, this);
  }

  ~LimLog() {
    {
      // notify background thread befor the object detoryed.
      std::unique_lock<std::mutex> lock(condMutex_);
      threadSync_ = true;
      proceedCond_.notify_all();
      hitEmptyCond_.wait(lock);
    }

    {
      // stop sink thread.
      std::lock_guard<std::mutex> lock(condMutex_);
      threadExit_ = true;
      proceedCond_.notify_all();
    }

    if (sinkThread_.joinable())
      sinkThread_.join();

    free(outputBuffer_);
    free(doubleBuffer_);

    for (auto &buf : threadBuffers_)
      free(buf);

    listStatistic();
  }

  /// Consume the log info produced by the front end to the file.
  void sinkThreadFunc() {
    while (!threadExit_) {
      // move front-end data to internal buffer.
      {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        uint32_t bbufferIdx = 0;
        // while (!threadExit_ && !outputFull_ && !threadBuffers_.empty()) {
        while (!threadExit_ && !outputFull_ &&
               (bbufferIdx < threadBuffers_.size())) {
          BlockingBuffer *bbuffer = threadBuffers_[bbufferIdx];
          uint32_t consumableBytes = bbuffer->consumable();

          if (bufferSize_ - perConsumeBytes_ < consumableBytes) {
            outputFull_ = true;
            break;
          }

          if (consumableBytes > 0) {
            uint32_t n = bbuffer->consume(outputBuffer_ + perConsumeBytes_,
                                          consumableBytes);
            perConsumeBytes_ += n;
          } else {
            // threadBuffers_.erase(threadBuffers_.begin() +
            // bbufferIdx);
          }
          bbufferIdx++;
        }
      }

      // not data to sink, go to sleep, 50us.
      if (perConsumeBytes_ == 0) {
        std::unique_lock<std::mutex> lock(condMutex_);

        // if front-end generated sync operation, consume again.
        if (threadSync_) {
          threadSync_ = false;
          continue;
        }

        hitEmptyCond_.notify_one();
        proceedCond_.wait_for(lock, std::chrono::microseconds(50));
      } else {
        uint64_t beginTime = detail::Timestamp::now().timestamp();
        writer_->write(outputBuffer_, perConsumeBytes_);
        uint64_t endTime = detail::Timestamp::now().timestamp();

        totalSinkTimes_ += endTime - beginTime;
        sinkCount_++;
        totalConsumeBytes_ += perConsumeBytes_;
        perConsumeBytes_ = 0;
        outputFull_ = false;
      }
    }
  }

  BlockingBuffer *blockingBuffer() {
    static thread_local BlockingBuffer *buf = nullptr;
    if (!buf) {
      std::lock_guard<std::mutex> lock(bufferMutex_);
      buf = static_cast<BlockingBuffer *>(malloc(sizeof(BlockingBuffer)));
      threadBuffers_.push_back(buf);
    }
    return buf;
  }

private:
  std::unique_ptr<Writer> writer_;
  bool threadSync_; // front-back-end sync.
  bool threadExit_; // background thread exit flag.
  bool outputFull_; // output buffer full flag.
  LogLevel level_;
  uint32_t sinkCount_;             // count of sinking to file.
  std::atomic<uint64_t> logCount_; // count of produced logs.
  uint64_t totalSinkTimes_;        // total time takes of sinking to file.
  uint64_t totalConsumeBytes_;     // total consume bytes.
  uint32_t perConsumeBytes_;       // bytes of consume first-end data per loop.
  uint32_t bufferSize_;            // two buffer size.
  char *outputBuffer_;             // first internal buffer.
  char *doubleBuffer_;             // second internal buffer.
  std::vector<BlockingBuffer *> threadBuffers_;
  std::thread sinkThread_;
  std::mutex bufferMutex_; // internel buffer mutex.
  std::mutex condMutex_;
  std::condition_variable proceedCond_;  // for background thread to proceed.
  std::condition_variable hitEmptyCond_; // for no data to consume.

  static const uint32_t kMaxFileSize = 64; // MB
  static const uint32_t kMaxFileCount = 16;
  static constexpr const char *kFileName = "log";
};

/// Log Location, include file, function, line.
struct LogLoc {
public:
  LogLoc() : LogLoc("", "", 0) {}

  LogLoc(const char *file, const char *function, uint32_t line)
      : file_(file), function_(function), line_(line) {}

  bool empty() const { return line_ == 0; }

  const char *file_;
  const char *function_;
  uint32_t line_;
};

/// Set log level, defalt WARN.
inline void setLogLevel(LogLevel level) {
  LimLog::singleton()->setLogLevel(level);
}

/// Get log level.
inline LogLevel getLogLevel() { return LimLog::singleton()->getLogLevel(); }

/// Set log file (basename and dir, no suffix), defalut `./run`.
/// The Program will automatically add suffix (`.log`).
inline void setLogFile(const char *file) {
  LimLog::singleton()->setLogFile(file);
}

/// Set max log file size (MB).
inline void setMaxSize(size_t nMB) { LimLog::singleton()->setMaxSize(nMB); }

/// Set max log file count.
inline void setMaxBackups(size_t count) {
  LimLog::singleton()->setMaxBackups(count);
}

inline void setWriter(std::unique_ptr<limlog::Writer> w) {
  LimLog::singleton()->setWriter(std::move(w));
}

/// Back-end provides produce interface.
inline void produceLog(const char *data, size_t n) {
  LimLog::singleton()->produce(data, n);
}

/// Move consumable position with increaing to ensure a complete log is consumed
/// each time.
inline void incConsumablePos(uint32_t n) {
  LimLog::singleton()->incConsumable(n);
}

/// A line log info, usage is same as 'std::cout'.
// Log format in memory.
//  +------+-----------+-------+------+------+----------+------+
//  | time | thread id | level | logs | file | function | line |
//  +------+-----------+-------+------+------+----------+------+
class LogLine {
public:
  LogLine(LogLevel level, const LogLoc &loc) : count_(0), loc_(loc) {
    *this << detail::Timestamp::now().format() << ' ' << gettid() << ' '
          << stringifyLogLevel(level) << "  ";
  }

  ~LogLine() {
    *this << loc_ << '\n';
    incConsumablePos(count_); // already produce a complete log.
  }

  /// Overloaded functions with various types of argument.
  LogLine &operator<<(bool arg) {
    if (arg)
      append("true", 4);
    else
      append("false", 5);
    return *this;
  }

  LogLine &operator<<(char arg) {
    append(&arg, 1);
    return *this;
  }

  LogLine &operator<<(int16_t arg) {
    char tmp[8];
    size_t len = detail::i16toa(arg, tmp);
    append(tmp, len);
    return *this;
  }

  LogLine &operator<<(uint16_t arg) {
    char tmp[8];
    size_t len = detail::u16toa(arg, tmp);
    append(tmp, len);
    return *this;
  }

  LogLine &operator<<(int32_t arg) {
    char tmp[12];
    size_t len = detail::i32toa(arg, tmp);
    append(tmp, len);
    return *this;
  }

  LogLine &operator<<(uint32_t arg) {
    char tmp[12];
    size_t len = detail::u32toa(arg, tmp);
    append(tmp, len);
    return *this;
  }

  LogLine &operator<<(int64_t arg) {
    char tmp[24];
    size_t len = detail::i64toa(arg, tmp);
    append(tmp, len);
    return *this;
  }

  LogLine &operator<<(uint64_t arg) {
    char tmp[24];
    size_t len = detail::u64toa(arg, tmp);
    append(tmp, len);
    return *this;
  }

  LogLine &operator<<(double arg) {
    append(std::to_string(arg).c_str());
    return *this;
  }

  LogLine &operator<<(const char *arg) {
    append(arg);
    return *this;
  }

  LogLine &operator<<(const std::string &arg) {
    append(arg.c_str(), arg.length());
    return *this;
  }

  LogLine &operator<<(const LogLoc &loc) {
    if (!loc.empty())
      *this << "  " << loc.file_ << ':' << loc.function_ << "():" << loc.line_;
    return *this;
  }

private:
  void append(const char *data, size_t n) {
    produceLog(data, n);
    count_ += static_cast<uint32_t>(n);
  }
  void append(const char *data) { append(data, strlen(data)); }

  uint32_t count_; // count of a log line bytes.
  LogLoc loc_;
};

} // namespace limlog

/// Custom macro \p __REL_FILE__ is relative file path as filename.
#ifndef __REL_FILE__
#define __REL_FILE__ __FILE__
#endif

/// Create a logline with log level \p level and the log location \p loc .
#define LOG(level, loc)                                                        \
  if (limlog::getLogLevel() <= level)                                          \
  limlog::LogLine(level, loc)

/// Create a logline with log level \p level , but without log localtion.
#define LOG_N_LOC(level) LOG(level, limlog::LogLoc())

#define LOG_TRACE LOG_N_LOC(limlog::LogLevel::TRACE)
#define LOG_DEBUG LOG_N_LOC(limlog::LogLevel::DEBUG)
#define LOG_INFO LOG_N_LOC(limlog::LogLevel::INFO)
#define LOG_WARN LOG_N_LOC(limlog::LogLevel::WARN)
#define LOG_ERROR LOG_N_LOC(limlog::LogLevel::ERROR)
#define LOG_FATAL LOG_N_LOC(limlog::LogLevel::FATAL)

/// Create a logline with log level \p level and the log localtion.
#define LOG_LOC(level)                                                         \
  LOG(level, limlog::LogLoc(__REL_FILE__, __FUNCTION__, __LINE__))

#define LIM_LOG_TRACE LOG_LOC(limlog::LogLevel::TRACE)
#define LIM_LOG_DEBUG LOG_LOC(limlog::LogLevel::DEBUG)
#define LIM_LOG_INFO LOG_LOC(limlog::LogLevel::INFO)
#define LIM_LOG_WARN LOG_LOC(limlog::LogLevel::WARN)
#define LIM_LOG_ERROR LOG_LOC(limlog::LogLevel::ERROR)
#define LIM_LOG_FATAL LOG_LOC(limlog::LogLevel::FATAL)