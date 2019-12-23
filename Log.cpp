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

#include <chrono>

#include <unistd.h>
#include <sys/syscall.h>

#include <stdlib.h>

namespace limlog {

thread_local BlockingBuffer *LimLog::blockingBuffer_ = nullptr;
LimLog LimLog::singletonLog;

thread_local pid_t tid = 0;

// \TODO get thread id in windows.
pid_t gettid() {
    if (tid == 0)
        tid = syscall(__NR_gettid);
    return tid;
}

// LogSink constructor
LogSink::LogSink()
    : fileCount_(0),
      rollSize_(10),
      writtenBytes_(0),
      fileName_(kDefaultLogFile),
      fp_(nullptr) {
    rollFile();
}

LogSink::LogSink(uint32_t rollSize)
    : fileCount_(0),
      rollSize_(rollSize),
      writtenBytes_(0),
      fileName_(kDefaultLogFile),
      fp_(nullptr) {
    rollFile();
}

LogSink::~LogSink() {
    if (fp_)
        fclose(fp_);
}

// Set log file should reopen file.
void LogSink::setLogFile(const char *file) {
    fileName_.assign(file);
    rollFile();
}

// \TODO auto roll file with time.
void LogSink::rollFile() {
    if (fp_)
        fclose(fp_);

    std::string file;
    if (fileName_ == kDefaultLogFile) {
        // default format, `./run.log`
        file = fileName_ + ".log";
    } else {
        std::string time = util::Timestamp::now().time();
        std::string date = util::Timestamp::now().date();
        // use setting file format, `dir/file.date.time.filecount.log`
        file = fileName_ + '.' + date + '.' + time + '.' +
               std::to_string(fileCount_) + ".log";
    }

    fp_ = fopen(file.c_str(), "a+");
    if (!fp_) {
        fprintf(stderr, "Faild to open file:%s\n", file.c_str());
        exit(-1);
    }

    fileCount_++;
}

ssize_t LogSink::sink(const char *data, size_t len) {
    uint64_t rollBytes = rollSize_ * kBytesPerMb;
    if (writtenBytes_ % rollBytes + len > rollBytes)
        rollFile();

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

    writtenBytes_ += len - remain;

    return len - remain;
}

// used() may be called by different threads, so add memory barrier to ensure
// the lasted *Pos_ is read.
uint32_t BlockingBuffer::used() const {
    asm volatile("lfence" ::: "memory");
    return producePos_ - consumePos_;
}

void BlockingBuffer::incConsumablePos(uint32_t n) {
    asm volatile("sfence" ::: "memory");
    consumablePos_ += n;
};

uint32_t BlockingBuffer::consumable() const {
    asm volatile("lfence" ::: "memory");
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

    asm volatile("sfence" ::: "memory");

    consumePos_ += avail;

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

    asm volatile("sfence" ::: "memory");

    produceCount_++;
    producePos_ += n;
}

// LimLog Constructor.
LimLog::LimLog()
    : logSink_(),
      threadSync_(false),
      threadExit_(false),
      outputFull_(false),
      level_(LogLevel::WARN),
      bufferSize_(1 << 24),
      sinkCount_(0),
      perConsumeBytes_(0),
      totalConsumeBytes_(0),
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
        std::unique_lock<std::mutex> lock(singletonLog.condMutex_);
        singletonLog.threadSync_ = true;
        singletonLog.proceedCond_.notify_all();
        singletonLog.hitEmptyCond_.wait(lock);
    }

    {
        // stop sink thread.
        std::lock_guard<std::mutex> lock(condMutex_);
        singletonLog.threadExit_ = true;
        singletonLog.proceedCond_.notify_all();
    }

    if (singletonLog.sinkThread_.joinable())
        singletonLog.sinkThread_.join();

    free(outputBuffer_);
    free(doubleBuffer_);

    for (auto &buf : threadBuffers_)
        free(buf);

    singletonLog.listStatistic();
}

// Sink log info to file with async.
void LimLog::sinkThreadFunc() {
    // \fixed me, it will enter infinity if compile with -O3 .
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
            logSink_.sink(outputBuffer_, perConsumeBytes_);
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

    blockingBuffer_->produce(data, n);
}

void LimLog::incConsumable(uint32_t n) { blockingBuffer_->incConsumablePos(n); }

// List related data of loggin system.
// \TODO add average sink time.
void LimLog::listStatistic() const {
    printf("=== Logging System Related Data ===\n");
    printf("  Sink data to file count: [%u]\n", sinkCount_);
    printf("  Total sink bytes: [%lu]\n", totalConsumeBytes_);
    printf("  Average sink bytes: [%lu]\n\n",
           sinkCount_ == 0 ? 0 : totalConsumeBytes_ / sinkCount_);
}

LogLine::LogLine(LogLevel level, const char *file, const char *function,
                 uint32_t line)
    : file_((file)), function_(function), line_(line), count_(0) {
    *this << util::Timestamp::now().formatTimestamp() << ' ' << gettid() << ' '
          << stringifyLogLevel(level) << "  ";
}

LogLine::~LogLine() {
    *this << " - " << file_ << ':' << function_
          << "():" << std::to_string(line_) << '\n';
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

LogLine &LogLine::operator<<(int32_t arg) {
    append(std::to_string(arg).c_str());
    return *this;
}

LogLine &LogLine::operator<<(uint32_t arg) {
    append(std::to_string(arg).c_str());
    return *this;
}

LogLine &LogLine::operator<<(int64_t arg) {
    append(std::to_string(arg).c_str());
    return *this;
}

LogLine &LogLine::operator<<(uint64_t arg) {
    append(std::to_string(arg).c_str());
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

// Set related properties of logging.
void setLogLevel(LogLevel level) { LimLog::singleton()->setLogLevel(level); }
void setLogFile(const char *file) { LimLog::singleton()->setLogFile(file); }
void setRollSize(uint32_t nMb) { LimLog::singleton()->setRollSize(nMb); }

LogLevel getLogLevel() { return LimLog::singleton()->getLogLevel(); }

// Stringify log level with width of 5.
std::string stringifyLogLevel(LogLevel level) {
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
