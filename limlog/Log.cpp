//===- Log.cpp - Logging Implementation -------------------------*- C++ -*-===//
//
/// \file
/// This file contains the processing of logs by background thread and
/// organization of log fomat.
//
// Author:  zxh(definezxh@163.com)
// Date:    2019/12/07 15:36:49
//===----------------------------------------------------------------------===//

#include "Log.h"
#include "Timestamp.h"
#include "NumToString.h"

#include <algorithm>
#include <chrono>

#ifdef __linux
#include <unistd.h>
#include <sys/syscall.h> // gettid().
typedef pid_t thread_id_t;
#else
#include <sstream>
typedef unsigned int thread_id_t; // MSVC
#endif

#include <stdlib.h>

namespace limlog {

thread_local BlockingBuffer *LimLog::blockingBuffer_ = nullptr;

thread_local thread_id_t t_tid = 0;

thread_id_t gettid() {
    if (t_tid == 0) {
#ifdef __linux
        t_tid = syscall(__NR_gettid);
#else
        std::stringstream ss;
        ss << std::this_thread::get_id();
        ss >> t_tid;
#endif
    }
    return t_tid;
}

// LogSink constructor
LogSink::LogSink() : LogSink(10) {}

LogSink::LogSink(uint32_t rollSize)
    : fileCount_(0),
      rollSize_(rollSize),
      writtenBytes_(0),
      fileName_(kDefaultLogFile),
      date_(util::Timestamp::now().date()),
      fp_(nullptr) {}

LogSink::~LogSink() {
    if (fp_)
        fclose(fp_);
}

// Set log file should reopen file.
void LogSink::setLogFile(const char *file) {
    fileName_.assign(file);
    rollFile();
}

void LogSink::rollFile() {
    if (fp_)
        fclose(fp_);

    std::string file(fileName_ + '.' + date_);
    if (fileCount_)
        file += '.' + std::to_string(fileCount_) + ".log";
    else
        file += ".log";

    fp_ = fopen(file.c_str(), "a+");
    if (!fp_) {
        fprintf(stderr, "Faild to open file:%s\n", file.c_str());
        exit(-1);
    }

    fileCount_++;
}

size_t LogSink::sink(const char *data, size_t len) {
    if (fp_ == nullptr)
        rollFile();

    std::string today(util::Timestamp::now().date());
    if (date_.compare(today)) {
        date_.assign(today);
        fileCount_ = 0;
        rollFile();
    }

    uint64_t rollBytes = rollSize_ * kBytesPerMb;
    if (writtenBytes_ % rollBytes + len > rollBytes)
        rollFile();

    return write(data, len);
}

size_t LogSink::write(const char *data, size_t len) {
    size_t n = fwrite(data, 1, len, fp_);
    size_t remain = len - n;
    while (remain > 0) {
        size_t x = fwrite(data + n, 1, remain, fp_);
        if (x == 0) {
            int err = ferror(fp_);
            if (err)
                fprintf(stderr, "File write error: %s\n", strerror(err));
            break;
        }
        n += x;
        remain -= x;
    }

    fflush(fp_);
    writtenBytes_ += len - remain;

    return len - remain;
}

// used() may be called by different threads, so add memory barrier to ensure
// the lasted *Pos_ is read.
uint32_t BlockingBuffer::used() const {
    std::atomic_thread_fence(std::memory_order_acquire);
    return producePos_ - consumePos_;
}

void BlockingBuffer::incConsumablePos(uint32_t n) {
    consumablePos_ += n;
    std::atomic_thread_fence(std::memory_order_release);
};

uint32_t BlockingBuffer::consumable() const {
    std::atomic_thread_fence(std::memory_order_acquire);
    return consumablePos_ - consumePos_;
}

// Get the log info from buffer.
uint32_t BlockingBuffer::consume(char *to, uint32_t n) {
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

// Put the log info to buffer with blocking.
void BlockingBuffer::produce(const char *from, uint32_t n) {
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

// LimLog Constructor.
LimLog::LimLog()
    : logSink_(),
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

LimLog::~LimLog() {
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

LimLog *LimLog::singleton() {
    static LimLog s_limlog;
    return &s_limlog;
}

// Sink log info to file with async.
void LimLog::sinkThreadFunc() {
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
                    uint32_t n = bbuffer->consume(
                        outputBuffer_ + perConsumeBytes_, consumableBytes);
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
            uint64_t beginTime = util::Timestamp::now().timestamp();
            logSink_.sink(outputBuffer_, perConsumeBytes_);
            uint64_t endTime = util::Timestamp::now().timestamp();

            totalSinkTimes_ += endTime - beginTime;
            sinkCount_++;
            totalConsumeBytes_ += perConsumeBytes_;
            perConsumeBytes_ = 0;
            outputFull_ = false;
        }
    }
}

void LimLog::produce(const char *data, size_t n) {
    if (!blockingBuffer_) {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        blockingBuffer_ =
            static_cast<BlockingBuffer *>(malloc(sizeof(BlockingBuffer)));
        threadBuffers_.push_back(blockingBuffer_);
    }

    blockingBuffer_->produce(data, static_cast<uint32_t>(n));
}

void LimLog::incConsumable(uint32_t n) {
    blockingBuffer_->incConsumablePos(n);
    logCount_.fetch_add(1, std::memory_order_relaxed);
}

// List related data of loggin system.
// \TODO write this info to file and stdout with stream.
void LimLog::listStatistic() const {
    printf("\n");
    printf("=== Logging System Related Data ===\n");
    printf("  Total produced logs count: [%ju].\n", logCount_.load());
    printf("  Total bytes of sinking to file: [%ju] bytes.\n",
           totalConsumeBytes_);
    printf("  Average bytes of sinking to file: [%ju] bytes.\n",
           sinkCount_ == 0 ? 0 : totalConsumeBytes_ / sinkCount_);
    printf("  Count of sinking to file: [%u].\n", sinkCount_);
    printf("  Total microseconds takes of sinking to file: [%ju] us.\n",
           totalSinkTimes_);
    printf("  Average microseconds takes of sinking to file: [%ju] us.\n",
           sinkCount_ == 0 ? 0 : totalSinkTimes_ / sinkCount_);
    printf("\n");
}

LogLine::LogLine(LogLevel level, const LogLoc &loc) : count_(0), loc_(loc) {
    *this << util::Timestamp::now().formatTimestamp() << ' ' << gettid() << ' '
          << stringifyLogLevel(level) << "  ";
}

LogLine::~LogLine() {
    *this << loc_ << '\n';
    incConsumablePos(count_); // already produce a complete log.
}

void LogLine::append(const char *data, size_t n) {
    produceLog(data, n);
    count_ += static_cast<uint32_t>(n);
}
void LogLine::append(const char *data) { append(data, strlen(data)); }

LogLine &LogLine::operator<<(bool arg) {
    if (arg)
        append("true", 4);
    else
        append("false", 5);
    return *this;
}

LogLine &LogLine::operator<<(char arg) {
    append(&arg, 1);
    return *this;
}

LogLine &LogLine::operator<<(int16_t arg) {
    char tmp[8];
    size_t len = util::i16toa(arg, tmp);
    append(tmp, len);
    return *this;
}

LogLine &LogLine::operator<<(uint16_t arg) {
    char tmp[8];
    size_t len = util::u16toa(arg, tmp);
    append(tmp, len);
    return *this;
}

LogLine &LogLine::operator<<(int32_t arg) {
    char tmp[12];
    size_t len = util::i32toa(arg, tmp);
    append(tmp, len);
    return *this;
}

LogLine &LogLine::operator<<(uint32_t arg) {
    char tmp[12];
    size_t len = util::u32toa(arg, tmp);
    append(tmp, len);
    return *this;
}

LogLine &LogLine::operator<<(int64_t arg) {
    char tmp[24];
    size_t len = util::i64toa(arg, tmp);
    append(tmp, len);
    return *this;
}

LogLine &LogLine::operator<<(uint64_t arg) {
    char tmp[24];
    size_t len = util::u64toa(arg, tmp);
    append(tmp, len);
    return *this;
}

LogLine &LogLine::operator<<(double arg) {
    append(std::to_string(arg).c_str());
    return *this;
}

LogLine &LogLine::operator<<(const char *arg) {
    append(arg);
    return *this;
}

LogLine &LogLine::operator<<(const std::string &arg) {
    append(arg.c_str(), arg.length());
    return *this;
}

LogLine &LogLine::operator<<(const LogLoc &loc) {
    if (!loc.empty())
        *this << "  " << loc.file_ << ':' << loc.function_
              << "():" << loc.line_;
    return *this;
}

// Set related properties of logging.
void setLogLevel(LogLevel level) { LimLog::singleton()->setLogLevel(level); }
void setLogFile(const char *file) { LimLog::singleton()->setLogFile(file); }
void setRollSize(uint32_t nMb) { LimLog::singleton()->setRollSize(nMb); }

LogLevel getLogLevel() { return LimLog::singleton()->getLogLevel(); }

// Stringify log level with width of 5.
const char *stringifyLogLevel(LogLevel level) {
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

void produceLog(const char *data, size_t n) {
    LimLog::singleton()->produce(data, n);
}

void incConsumablePos(uint32_t n) { LimLog::singleton()->incConsumable(n); }

} // namespace limlog
