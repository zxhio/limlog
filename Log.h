//===- Log.h - Logging Interface --------------------------------*- C++ -*-===//
//
/// \file
/// A simple and fast logger with asynchronization.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/07 14:13:09
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <stdint.h> // uint32_t
#include <stdio.h>
#include <string.h>

namespace limlog {

/// Log level >= set level will be written to file.
enum LogLevel : uint8_t { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

/// Sink log info on file, and the file automatic roll with a setting size.
/// If set log file, the log file format is
/// `dir/filename.date.time.filecount.log `, default is `./run.log`.
class LogSink {
  public:
    LogSink();
    LogSink(uint32_t rollSize);
    ~LogSink();

    void setLogFile(const char *file);
    void setRollSize(uint32_t size) { rollSize_ = size; }

    void rollFile();
    ssize_t sink(const char *data, size_t len);

  private:
    uint32_t fileCount_;    // file roll count.
    uint32_t rollSize_;     // size of MB.
    uint64_t writtenBytes_; // total written bytes.
    std::string fileName_;
    FILE *fp_;
    static const uint32_t kBytesPerMb = 1 << 20;
    static constexpr const char *kDefaultLogFile = "./run";
};

/// Circle FIFO blocking produce/consume byte queue. Hold log info to wait for
/// background thread consume. It exists in each thread.
//  \TODO, add memory barrier to avoid compiler optimization with -O3.
class BlockingBuffer {
  public:
    BlockingBuffer() : producePos_(0), consumePos_(0) {}

    /// Get position offset calculated from buffer start.
    uint32_t offsetOfPos(uint32_t pos) const { return pos & (size() - 1); }

    /// Buffer size.
    uint32_t size() const { return kBlockingBufferSize; }

    /// Already used bytes.
    uint32_t used() const;
    uint32_t unused() const { return kBlockingBufferSize - used(); }
    void reset() { producePos_ = consumePos_ = 0; }

    /// Peek the produce position in buffer.
    char *peek() { return &storage_[producePos_]; }

    /// consume n bytes data and only move the consume position.
    void consume(uint32_t n) { consumePos_ += n; }

    /// consume n bytes data to \p to .
    uint32_t consume(char *to, uint32_t n);

    /// Copy \p n bytes log info from \p from to buffer. It will be blocking
    /// when buffer space is insufficient.
    void produce(const char *from, uint32_t n);

  private:
    static const uint32_t kBlockingBufferSize = 1 << 20; // 1 MB
    uint32_t producePos_;
    uint32_t consumePos_;
    uint32_t produceCount_;
    char storage_[kBlockingBufferSize]; // buffer size power of 2.
};

/// Logging system.
/// Manage the action and attibute of logging.
class LimLog {
  public:
    /// Log setting.
    LogLevel getLogLevel() const { return level_; };
    void setLogLevel(LogLevel level) { level_ = level; }
    void setLogFile(const char *file) { logSink_.setLogFile(file); };
    void setRollSize(uint32_t size) { logSink_.setRollSize(size); };

    /// Singleton pointer.
    static LimLog *singleton() { return &singletonLog; }

    /// Produce log data to \p BlockingBuffer in each thread.
    void produce(const char *data, size_t n);

    /// List log related statistic.
    void listStatistic() const;

  private:
    LimLog();
    LimLog(const LimLog &) = delete;
    LimLog &operator=(const LimLog &) = delete;
    ~LimLog();

    /// Consume the log info produced by the front end to the file.
    void sinkThreadFunc();

  private:
    LogSink logSink_;
    bool threadSync_; // front-back-end sync.
    bool threadExit_; // background thread exit flag.
    bool outputFull_; // output buffer full flag.
    LogLevel level_;
    uint32_t bufferSize_;        // two buffer size.
    uint32_t sinkCount_;         // count of sink to file.
    uint32_t perConsumeBytes_;   // bytes of consume first-end data per loop.
    uint64_t totalConsumeBytes_; // total consume bytes.
    char *outputBuffer_;         // first internal buffer.
    char *doubleBuffer_;         // second internal buffer.
    std::vector<BlockingBuffer *> threadBuffers_;
    std::thread sinkThread_;
    std::mutex bufferMutex_; // internel buffer mutex.
    std::mutex condMutex_;
    std::condition_variable proceedCond_;  // for background thread to proceed.
    std::condition_variable hitEmptyCond_; // for no data to consume.
    static LimLog singletonLog;
    static thread_local BlockingBuffer *blockingBuffer_;
};

/// Set log level, defalt WARN.
void setLogLevel(LogLevel level);

/// Get log level.
LogLevel getLogLevel();
std::string stringifyLogLevel(LogLevel level);

/// Set log file (basename and dir, no suffix), defalut `./run`.
/// The Program will automatically add suffix (`.log`).
void setLogFile(const char *file);

/// Set log file roll size (MB), default 10 MB.
void setRollSize(uint32_t nMb);

/// Back-end provides produce interface.
void produceLog(const char *data, size_t n);

/// List logging system related data.
// void listLogStatistic();

} // namespace limlog
