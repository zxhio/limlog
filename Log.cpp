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

#include <chrono>
#include <cstdlib>

namespace limlog {

thread_local BlockingBuffer *LimLog::blockingBuffer_ = nullptr;
LimLog LimLog::singletonLog;

// LogSink constructor
LogSink::LogSink()
    : fileCount_(0),
      rollSize_(10),
      writtenBytes_(0),
      fileName_("./limlog"),
      fp_(nullptr) {
    rollFile();
}

LogSink::LogSink(uint32_t rollSize)
    : fileCount_(0),
      rollSize_(rollSize),
      writtenBytes_(0),
      fileName_("./limlog"),
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

// \TODO auto roll file with size and time.
// now only reopen file.
void LogSink::rollFile() {
    if (fp_)
        fclose(fp_);

    std::string file(fileName_);

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

// Get the log info from buffer.
uint32_t BlockingBuffer::consume(char *to, uint32_t n) {
    // available bytes to consume.
    uint32_t avail = std::min(used(), n);

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
      bufferMutex_(),
      condMutex_(),
      proceedCond_(),
      hitEmptyCond_(),
      sinkThread_() {

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
                uint32_t usedBytes = bbuffer->used();

                if (bufferSize_ - perConsumeBytes_ < usedBytes) {
                    outputFull_ = true;
                    break;
                }

                if (usedBytes > 0) {
                    uint32_t n = bbuffer->consume(
                        outputBuffer_ + perConsumeBytes_, usedBytes);
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

// List related data of loggin system.
// \TODO add average sink time.
void LimLog::listStatistic() const {
    printf("=== Logging System Related Data ===\n");
    printf("  Sink data to file count: [%lu]\n", sinkCount_);
    printf("  Total sink bytes: [%llu]\n", totalConsumeBytes_);
    printf("  Average sink bytes: [%lu]\n\n",
           sinkCount_ == 0 ? 0 : totalConsumeBytes_ / sinkCount_);
}

// Set related properties of logging.
void setLogLevel(LogLevel level) { LimLog::singleton()->setLogLevel(level); }
void setLogFile(const char *file) { LimLog::singleton()->setLogFile(file); }
void setRollSize(uint32_t nMb) { LimLog::singleton()->setRollSize(nMb); }

// Stringify log level with width of 5.
std::string stringifyLogLevel() {
    LogLevel level = LimLog::singleton()->getLogLevel();
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

} // namespace limlog
