//===- limlog.h - LimLog ----------------------------------------*- C++ -*-===//
//
/// \file
/// Lightweight, easy-to-use and fast logging library, providing only a
/// front-end for log writing.
//
// Author:  zxh
// Date:    2022/02/15 22:26:09
//===----------------------------------------------------------------------===//

#pragma once

#include <string.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

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

// The digits table is used to look up for number within 100.
// Each two character corresponds to one digit and ten digits.
static constexpr char DigitsTable[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0',
    '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2',
    '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3',
    '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5',
    '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6',
    '7', '6', '8', '6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8', '0', '8', '1', '8',
    '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9',
    '7', '9', '8', '9', '9'};

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatUIntInternal(T v, char to[]) {
  char *p = to;

  while (v >= 100) {
    const unsigned idx = (v % 100) << 1;
    v /= 100;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  if (v < 10) {
    *p++ = v + '0';
  } else {
    const unsigned idx = v << 1;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  return p - to;
}

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatSIntInternal(T v, char to[]) {
  char *p = to;

  while (v <= static_cast<T>(-100)) {
    const T idx = -(v % 100) * 2;
    v /= 100;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  if (v > static_cast<T>(-10)) {
    *p++ = -v + '0';
  } else {
    const T idx = -v * 2;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  return p - to;
}

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatInt(T v, char *to) {
  char buf[sizeof(v) * 4];
  size_t signLen = 0;
  size_t intLen = 0;

  if (v < 0) {
    *to++ = '-';
    signLen = 1;
    intLen = formatSIntInternal(v, buf);
  } else {
    intLen = formatUIntInternal(v, buf);
  }

  char *p = buf + intLen;
  for (size_t i = 0; i < intLen; ++i)
    *to++ = *--p;

  return signLen + intLen;
}

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatUIntWidth(T v, char *to, size_t fmtLen) {
  char buf[sizeof(v) * 4];
  size_t len = formatUIntInternal(v, buf);
  char *p = buf + len;

  for (size_t i = len; i < fmtLen; i++)
    *to++ = '0';

  size_t minLen = std::min(len, fmtLen);
  for (size_t i = 0; i < minLen; ++i)
    *to++ = *--p;

  return fmtLen;
}

static inline size_t formatChar(char *to, char c) {
  *to = c;
  return sizeof(char);
}

enum TimeFieldLen : size_t {
  Year = 4,
  Month = 2,
  Day = 2,
  Hour = 2,
  Minute = 2,
  Second = 2,
};

enum SecFracLen : size_t { Sec = 0, Milli = 3, Macro = 6, Nano = 9 };

class Time {
public:
  using TimePoint = std::chrono::time_point<std::chrono::system_clock,
                                            std::chrono::nanoseconds>;
  Time() = delete;
  Time(const Time &) = default;
  Time &operator=(const Time &) = default;

  explicit Time(time_t second)
      : Time(TimePoint(std::chrono::seconds(second))) {}
  explicit Time(const TimePoint &tp) : tp_(tp) {}

  /// Current time.
  static Time now() { return Time(std::chrono::system_clock::now()); }

  /// Year (4 digits, e.g. 1996).
  int year() const { return toTm().tm_year + 1900; }

  /// Month of then year, in the range [1, 12].
  int month() const { return toTm().tm_mon + 1; }

  /// Day of the month, in the range [1, 28/29/30/31].
  int day() const { return toTm().tm_mday; }

  /// Day of the week, in the range [1, 7].
  int weekday() const { return toTm().tm_wday; }

  /// Hour within day, in the range [0, 23].
  int hour() const { return toTm().tm_hour; }

  /// Minute offset within the hour, in the range [0, 59].
  int minute() const { return toTm().tm_min; }

  /// Second offset within the minute, in the range [0, 59].
  int second() const { return toTm().tm_sec; }

  /// Nanosecond offset within the second, in the range [0, 999999999].
  int nanosecond() const { return static_cast<int>(count() % std::nano::den); }

  /// Count of nanosecond elapsed since 1970-01-01T00:00:00Z .
  int64_t count() const { return tp_.time_since_epoch().count(); }

  /// Timezone name and offset in seconds east of UTC.
  std::pair<long int, std::string> timezone() const {
    static thread_local long int t_off = std::numeric_limits<long int>::min();
    static thread_local char t_zone[8];

    if (t_off == std::numeric_limits<long int>::min()) {
      struct tm t;
      time_t c = std::chrono::system_clock::to_time_t(tp_);
      localtime_r(&c, &t);
      t_off = t.tm_gmtoff;
      std::copy(t.tm_zone,
                t.tm_zone + std::char_traits<char>::length(t.tm_zone), t_zone);
    }

    return std::make_pair(t_off, t_zone);
  }

  /// Standard date-time full format using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58Z
  ///   2021-10-10T05:46:58+08:00
  std::string format() const { return formatInternal(SecFracLen::Sec); }

  /// Standard date-time format with millisecond using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58.123Z
  ///   2021-10-10T05:46:58.123+08:00
  std::string formatMilli() const { return formatInternal(SecFracLen::Milli); }

  /// Standard date-time format with macrosecond using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58.123456Z
  ///   2021-10-10T05:46:58.123456+08:00
  std::string formatMacro() const { return formatInternal(SecFracLen::Macro); }

  /// Standard date-time format with nanosecond using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58.123456789Z
  ///   2021-10-10T05:46:58.123456789+08:00
  std::string formatNano() const { return formatInternal(SecFracLen::Nano); }

private:
  struct tm toTm() const {
    struct tm t;
    time_t c = std::chrono::system_clock::to_time_t(tp_);

    // gmtime_r() is 150% faster than localtime_r() because it doesn't need to
    // calculate the time zone. For a more faster implementation refer to
    // musl-libc, which is 10% faster than the glibc gmtime_r().
    // ref:
    // https://git.musl-libc.org/cgit/musl/tree/src/time/gmtime_r.c?h=v1.2.2#n4
    // gmtime_r(&c, &t);
    localtime_r(&c, &t);

    return t;
  }

  std::string formatInternal(size_t fracLen) const {
    char datetime[40];
    char *p = datetime;
    struct tm t = toTm();

    p += formatDate(p, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    p += formatChar(p, 'T');
    p += formatTime(p, t.tm_hour, t.tm_min, t.tm_sec, fracLen);

    return std::string(datetime, p - datetime);
  }

  size_t formatDate(char *to, int year, int mon, int mday) const {
    char *p = to;
    p += formatUIntWidth(year, p, TimeFieldLen::Year);
    p += formatChar(p, '-');
    p += formatUIntWidth(mon, p, TimeFieldLen::Month);
    p += formatChar(p, '-');
    p += formatUIntWidth(mday, p, TimeFieldLen::Day);
    return p - to;
  }

  size_t formatTime(char *to, int hour, int min, int sec,
                    size_t fracLen) const {
    char *p = to;
    p += formatPartialTime(p, hour, min, sec, fracLen);
    p += formatTimeOff(p);
    return p - to;
  }

  size_t formatPartialTime(char *to, int hour, int min, int sec,
                           size_t fracLen) const {
    char *p = to;
    p += formatUIntWidth(hour, p, TimeFieldLen::Hour);
    p += formatChar(p, ':');
    p += formatUIntWidth(min, p, TimeFieldLen::Minute);
    p += formatChar(p, ':');
    p += formatUIntWidth(sec, p, TimeFieldLen::Second);
    p += formatSecFrac(p, nanosecond(), fracLen);
    return p - to;
  }

  size_t formatSecFrac(char *to, int frac, size_t fracLen) const {
    if (fracLen == 0 || frac == 0)
      return 0;

    char *p = to;
    p += formatChar(p, '.');
    p += formatUIntWidth(frac, p, fracLen);
    return p - to;
  }

  size_t formatTimeOff(char *to) const {
    long int off = timezone().first;
    char *p = to;

    if (off == 0) {
      p += formatChar(p, 'Z');
    } else {
      p += formatChar(p, off < 0 ? '-' : '+');
      p += formatUIntWidth(off / 3600, p, TimeFieldLen::Hour);
      p += formatChar(p, ':');
      p += formatUIntWidth(off % 3600, p, TimeFieldLen::Minute);
    }

    return p - to;
  }

  TimePoint tp_;
};

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

enum LogLevel : uint8_t { kTrace, kDebug, kInfo, kWarn, kError, kFatal };

// Stringify log level with width of 5.
static const char *stringifyLogLevel(LogLevel level) {
  const char *levelName[] = {"TRAC", "DEBU", "INFO", "WARN", "ERRO", "FATA"};
  return levelName[level];
}

/// Circle FIFO blocking produce/consume byte queue. Hold log info to wait for
/// background thread consume. It exists in each thread.
class BlockingBuffer {
public:
  BlockingBuffer() : producePos_(0), consumePos_(0), consumablePos_(0) {}

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
  void reset() {
    producePos_ = 0;
    consumePos_ = 0;
    consumablePos_ = 0;
  }

  /// The position at the end of the last complete log.
  uint32_t consumable() const {
    std::atomic_thread_fence(std::memory_order_acquire);
    return consumablePos_ - consumePos_;
  }

  /// Increase consumable position with a complete log length \a n .
  void incConsumablePos(uint32_t n) {
    consumablePos_ += n;
    std::atomic_thread_fence(std::memory_order_release);
  }

  /// Pointer to comsume position.
  char *data() { return &storage_[offsetOfPos(consumePos_)]; }

  /// Consume n bytes data and only move the consume position.
  void consume(uint32_t n) { consumePos_ += n; }

  /// Consume \a n bytes data to \a to .
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

  /// Copy \a n bytes log info from \a from to buffer. It will be blocking
  /// when buffer space is insufficient.
  void produce(const char *from, uint32_t n) {
    n = std::min(size(), n);
    while (unused() < n)
      /* blocking */;

    // offset of producePos to buffer end.
    uint32_t off2End = std::min(n, size() - offsetOfPos(producePos_));

    // first put the data starting from producePos until the end of buffer.
    memcpy(storage_ + offsetOfPos(producePos_), from, off2End);

    // then put the rest at beginning of the buffer.
    memcpy(storage_, from + off2End, n - off2End);

    producePos_ += n;
    std::atomic_thread_fence(std::memory_order_release);
  }

private:
  /// Get position offset calculated from buffer start.
  uint32_t offsetOfPos(uint32_t pos) const { return pos & (size() - 1); }

  static const uint32_t kBlockingBufferSize = 1024 * 1024 * 1; // 1 MB
  uint32_t producePos_;
  uint32_t consumePos_;
  uint32_t consumablePos_; // increase every time with a complete log length.
  char storage_[kBlockingBufferSize]; // buffer size power of 2.
};

using OutputFunc = ssize_t (*)(const char *, size_t);

struct StdoutWriter {
  static ssize_t write(const char *data, size_t n) {
    return fwrite(data, sizeof(char), n, stdout);
  }
};

struct NullWriter {
  static ssize_t write(const char *data, size_t n) { return 0; }
};

class SyncLogger {
public:
  SyncLogger() : output_(StdoutWriter::write) {}

  void setOutput(OutputFunc w) { output_ = w; }

  void produce(const char *data, size_t n) { buffer_.produce(data, n); }

  void flush(size_t n) {
    buffer_.incConsumablePos(n);
    output_(buffer_.data(), buffer_.consumable());
    buffer_.reset();
  }

private:
  OutputFunc output_;
  BlockingBuffer buffer_;
};

class AsyncLogger {
public:
  AsyncLogger() : output_(StdoutWriter::write) {}

  void setOutput(OutputFunc w) { output_ = w; }

  /// TODO:
  void produce(const char *data, size_t n) {}

  /// TODO:
  void flush(size_t n) {}

private:
  OutputFunc output_;
};

template <typename Logger> class LimLog {
public:
  LimLog() : level_(LogLevel::kInfo), output_(StdoutWriter::write) {}
  ~LimLog() {
    for (auto l : loggers_)
      delete (l);
  }
  LimLog(const LimLog &) = delete;
  LimLog &operator=(const LimLog &) = delete;

  /// Produce \a data which length \a n to BlockingBuffer in each thread.
  void produce(const char *data, size_t n) { logger()->produce(data, n); }

  /// Flush a logline with length \a n .
  void flush(size_t n) { logger()->flush(n); }

  /// Set log level \a level.
  void setLogLevel(LogLevel level) { level_ = level; }

  /// Get log level.
  LogLevel getLogLevel() const { return level_; }

  /// Set logger output \a w .
  void setOutput(OutputFunc w) {
    output_ = w;
    logger()->setOutput(w);
  }

private:
  Logger *logger() {
    static thread_local Logger *l = nullptr;
    if (!l) {
      std::lock_guard<std::mutex> lock(loggerMutex_);
      l = static_cast<Logger *>(new Logger);
      l->setOutput(output_);
      loggers_.push_back(l);
    }
    return l;
  }

  LogLevel level_;
  OutputFunc output_;
  std::mutex loggerMutex_;
  std::vector<Logger *> loggers_;
};

/// Singleton pointer.
static LimLog<SyncLogger> *singleton() {
  static LimLog<SyncLogger> s_limlog;
  return &s_limlog;
}

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
//  +-------+------+-----------+------+------+------------+------+
//  | level | time | thread id | logs | file | (function) | line |
//  +-------+------+-----------+------+------+------------+------+
class LogLine {
public:
  LogLine() = delete;
  LogLine(const LogLine &) = delete;
  LogLine &operator=(const LogLine &) = delete;

  LogLine(LogLevel level, const LogLoc &loc) : count_(0), loc_(loc) {
    *this << stringifyLogLevel(level) << ' ' << Time::now().formatMilli() << ' '
          << gettid() << loc_ << ' ';
  }

  ~LogLine() {
    *this << '\n';
    singleton()->flush(count_);
  }

  /// Overloaded `operator<<` for type various of integral num.
  template <typename T,
            typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
  LogLine &operator<<(T v) {
    char buf[sizeof(T) * 4];
    size_t len = formatInt(v, buf);
    append(buf, len);
    return *this;
  }

  /// Overloaded `operator<<` for type various of bool.
  LogLine &operator<<(bool v) {
    if (v)
      append("true", 4);
    else
      append("false", 5);
    return *this;
  }

  /// Overloaded `operator<<` for type various of char.
  LogLine &operator<<(char v) {
    append(&v, 1);
    return *this;
  }

  /// Overloaded `operator<<` for type various of float num.
  LogLine &operator<<(float v) {
    std::numeric_limits<float>::min();
    std::string s = std::to_string(v);
    append(s.data(), s.length());
    return *this;
  }

  /// Overloaded `operator<<` for type various of float num.
  LogLine &operator<<(double v) {
    std::string s = std::to_string(v);
    append(s.data(), s.length());
    return *this;
  }

  /// Overloaded `operator<<` for type various of char*.
  LogLine &operator<<(const char *v) {
    append(v);
    return *this;
  }

  /// Overloaded `operator<<` for type various of std::string.
  LogLine &operator<<(const std::string &v) {
    append(v.data(), v.length());
    return *this;
  }

  LogLine &operator<<(const LogLoc &loc) {
    if (!loc.empty())
      *this << ' ' << loc.file_ << ":" << loc.line_;
    return *this;
  }

private:
  void append(const char *data, size_t n) {
    singleton()->produce(data, n);
    count_ += n;
  }

  void append(const char *data) { append(data, strlen(data)); }

  size_t count_; // count of a log line bytes.
  LogLoc loc_;
};
} // namespace limlog

/// Create a logline with log level \a level and the log location \a loc .
#define LOG(level, loc)                                                        \
  if (limlog::singleton()->getLogLevel() <= level)                             \
  limlog::LogLine(level, loc)

/// Create a logline with log level \a level and the log localtion.
#define LOG_LOC(level)                                                         \
  LOG(level, limlog::LogLoc(__FILE__, __FUNCTION__, __LINE__))

#define LOG_TRACE LOG_LOC(limlog::LogLevel::kTrace)
#define LOG_DEBUG LOG_LOC(limlog::LogLevel::kDebug)
#define LOG_INFO LOG_LOC(limlog::LogLevel::kInfo)
#define LOG_WARN LOG_LOC(limlog::LogLevel::kWarn)
#define LOG_ERROR LOG_LOC(limlog::LogLevel::kError)
#define LOG_FATAL LOG_LOC(limlog::LogLevel::kFatal)