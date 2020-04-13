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
#include <atomic>

#include <stdint.h> // uint32_t
#include <stdio.h>
#include <string.h>

namespace limlog {

/// Log level >= set level will be written to file.
enum LogLevel : uint8_t { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

/// Sink log info on file, and the file automatic roll with a setting size.
/// If set log file, initially the log file format is `filename.date.log `.
class LogSink {
  public:
    LogSink();
    LogSink(uint32_t rollSize);
    ~LogSink();

    void setLogFile(const char *file);
    void setRollSize(uint32_t size) { rollSize_ = size; }

    /// Roll File with size. The new filename contain log file count.
    void rollFile();

    /// Sink log data \p data of length \p len to file.
    size_t sink(const char *data, size_t len);

  private:
    uint32_t fileCount_;    // file roll count.
    uint32_t rollSize_;     // size of MB.
    uint64_t writtenBytes_; // total written bytes.
    std::string fileName_;
    std::string date_;
    FILE *fp_;
    static const uint32_t kBytesPerMb = 1 << 20;
    static constexpr const char *kDefaultLogFile = "limlog";

    size_t write(const char *data, size_t len);
};

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
    uint32_t used() const;
    uint32_t unused() const { return kBlockingBufferSize - used(); }
    void reset() { producePos_ = consumePos_ = 0; }

    /// The position at the end of the last complete log.
    uint32_t consumable() const;

    /// Increase consumable position with a complete log length \p n .
    void incConsumablePos(uint32_t n);

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
    uint32_t consumablePos_; // increase every time with a complete log length.
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
    static LimLog *singleton();

    /// Produce log data to \p BlockingBuffer in each thread.
    void produce(const char *data, size_t n);

    /// Increase BlockingBuffer consumable position.
    void incConsumable(uint32_t n);

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
    uint32_t sinkCount_;             // count of sinking to file.
    std::atomic<uint64_t> logCount_; // count of produced logs.
    uint64_t totalSinkTimes_;        // total time takes of sinking to file.
    uint64_t totalConsumeBytes_;     // total consume bytes.
    uint32_t perConsumeBytes_; // bytes of consume first-end data per loop.
    uint32_t bufferSize_;      // two buffer size.
    char *outputBuffer_;       // first internal buffer.
    char *doubleBuffer_;       // second internal buffer.
    std::vector<BlockingBuffer *> threadBuffers_;
    std::thread sinkThread_;
    std::mutex bufferMutex_; // internel buffer mutex.
    std::mutex condMutex_;
    std::condition_variable proceedCond_;  // for background thread to proceed.
    std::condition_variable hitEmptyCond_; // for no data to consume.
    static thread_local BlockingBuffer *blockingBuffer_;
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

/// A line log info, usage is same as 'std::cout'.
// Log format in memory.
//  +------+-----------+-------+------+------+----------+------+
//  | time | thread id | level | logs | file | function | line |
//  +------+-----------+-------+------+------+----------+------+
class LogLine {
  public:
    LogLine(LogLevel level, const LogLoc &loc);
    ~LogLine();

    /// Overloaded functions with various types of argument.
    LogLine &operator<<(bool arg);
    LogLine &operator<<(char arg);
    LogLine &operator<<(int16_t arg);
    LogLine &operator<<(uint16_t arg);
    LogLine &operator<<(int32_t arg);
    LogLine &operator<<(uint32_t arg);
    LogLine &operator<<(int64_t arg);
    LogLine &operator<<(uint64_t arg);
    LogLine &operator<<(double arg);
    LogLine &operator<<(const char *arg);
    LogLine &operator<<(const std::string &arg);
    LogLine &operator<<(const LogLoc &loc);

  private:
    void append(const char *data, size_t len);
    void append(const char *data);

    uint32_t count_; // count of a log line bytes.
    LogLoc loc_;
};

/// Set log level, defalt WARN.
void setLogLevel(LogLevel level);

/// Get log level.
LogLevel getLogLevel();
const char *stringifyLogLevel(LogLevel level);

/// Set log file (basename and dir, no suffix), defalut `./run`.
/// The Program will automatically add suffix (`.log`).
void setLogFile(const char *file);

/// Set log file roll size (MB), default 10 MB.
void setRollSize(uint32_t nMb);

/// Back-end provides produce interface.
void produceLog(const char *data, size_t n);

/// List logging system related data.
// void listLogStatistic();

/// Move consumable position with increaing to ensure a complete log is consumed
/// each time.
void incConsumablePos(uint32_t n);

} // namespace limlog

/// Custom macro \p __REL_FILE__ is relative file path as filename.
#ifndef __REL_FILE__
#define __REL_FILE__ __FILE__
#endif

/// Create a logline with log level \p level and the log location \p loc .
#define LOG(level, loc)                                                        \
    if (limlog::getLogLevel() <= level)                                        \
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