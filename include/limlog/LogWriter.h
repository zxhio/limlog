//===- LogWriter.h - Log Writer ---------------------------------*- C++ -*-===//
//
/// \file
/// Log writer interface.
//
// Author:  zxh
// Date:    2021/04/09 14:56:30
//===----------------------------------------------------------------------===//

#include "Timestamp.h"

#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mutex>
#include <thread>
#include <vector>

namespace limlog {

const char kPathSeparator = '/';
const size_t kBytesPerMB = 1024 * 1024;
const uint64_t kDefaultMaxSize = 256;

std::string dir(const std::string &filename) {
  size_t lastSepIdx = filename.rfind(kPathSeparator);
  if (lastSepIdx != std::string::npos)
    return filename.substr(0, lastSepIdx);
  return ".";
}

std::string basename(const std::string &filename) {
  size_t lastSepIdx = filename.rfind(kPathSeparator);
  if (lastSepIdx != std::string::npos)
    return filename.substr(lastSepIdx + 1);
  return filename;
}

std::string ext(const std::string &filename) {
  size_t lastSepIdx = filename.rfind('.');
  if (lastSepIdx != std::string::npos)
    return filename.substr(lastSepIdx);
  return "";
}

ssize_t filesize(const std::string &filename) {
  struct stat st;
  ::stat(filename.c_str(), &st);
  return static_cast<ssize_t>(st.st_size);
}

bool exist(const std::string &filename) {
  return access(filename.c_str(), F_OK) == 0;
}

int mkdirAll(const std::string &path, mode_t mode) {
  struct stat st;
  if (::stat(path.c_str(), &st) == 0 && st.st_mode & S_IFDIR)
    return 0;

  size_t i = path.length();
  for (; i > 0 && path[i - 1] == '/'; i--)
    ;

  size_t j = i;
  for (; j > 0 && path[j - 1] != '/'; j--)
    ;

  if (j > 1) {
    if (mkdirAll(path.substr(0, j - 1), mode) != 0)
      return -1;
  }

  return ::mkdir(path.c_str(), mode);
}

std::vector<std::string> readDir(const std::string &d) {
  std::vector<std::string> files;

  DIR *dirp = opendir(d.c_str());
  if (dirp == nullptr) {
    fprintf(stderr, "fail to open dir: %s\n", strerror(errno));
    return files;
  }

  struct dirent *dp;
  while ((dp = readdir(dirp)) != nullptr) {
    if (dp->d_type & DT_REG) {
      files.push_back(dp->d_name);
    } else if (dp->d_type == DT_UNKNOWN) {
      char name[128];
      snprintf(name, sizeof(name), "%s/%s", d.c_str(), dp->d_name);
      struct stat st;
      if (::stat(name, &st) != 0) {
        continue;
      }
      if (S_ISREG(st.st_mode))
        files.push_back(dp->d_name);
    }
  }

  return files;
}

void copyMode(const std::string &oldfile, const std::string &newfile) {
#if defined(__linux) || defined(__APPLE__)
  mode_t mode = 0644;
  if (oldfile.length() != 0) {
    struct stat st;
    ::stat(oldfile.c_str(), &st);
    mode = st.st_mode;
  }
  ::chmod(newfile.c_str(), mode);
#endif
}

detail::Timestamp timeFromName(const std::string &file,
                               const std::string &prefix,
                               const std::string &suffix) {
  if (file.find(prefix) == std::string::npos)
    return detail::Timestamp();

  if (file.rfind(suffix) == std::string::npos)
    return detail::Timestamp();

  std::string ts =
      file.substr(prefix.size(), file.size() - suffix.size() - prefix.size());

  return detail::Timestamp::parse(ts);
}

struct LogFileInfo {
  std::string filename;
  detail::Timestamp ts;

  LogFileInfo(const std::string file, detail::Timestamp t)
      : filename(file), ts(t) {}
};

struct PathInfo {
  std::string prefix;
  std::string extPart;
  std::string basePart;
  std::string dirPart;

  PathInfo(const std::string &path) {
    dirPart = dir(path);
    basePart = basename(path);
    extPart = ext(basePart);
    prefix = basePart.substr(0, basePart.length() - extPart.length()) + '_';
  }
};

class Writer {
public:
  virtual ~Writer() = default;
  virtual ssize_t write(const char *data, size_t len) = 0;
  virtual void setFileName(const std::string &file) {}
  virtual void setMaxBackups(int backups) {}
  virtual void setMaxSize(uint64_t size) {}
};

class StdoutWriter : public Writer {
public:
  ssize_t write(const char *data, size_t len) override {
    return fprintf(stdout, "%s\n", data);
  }
};

class NullWriter : public Writer {
public:
  NullWriter() { fp_ = fopen("/dev/null", "a+"); }
  ~NullWriter() { fclose(fp_); }
  ssize_t write(const char *data, size_t len) override {
    return fwrite(data, sizeof(char), len, fp_);
  }

private:
  FILE *fp_;
};

class RotateWriter : public Writer {
public:
  RotateWriter(const std::string &file, uint64_t size, uint32_t age,
               uint32_t backups)
      : filename_(file),
        maxSize_(size),
        maxAge_(age),
        maxBackups_(backups),
        curSize_(0),
        fp_(nullptr),
        pathInfo_(filename()) {}

  ~RotateWriter() override {
    if (fp_ != nullptr)
      fclose(fp_);
  }

  void setMaxBackups(int backups) override { maxBackups_ = backups; }
  void setMaxSize(uint64_t size) override { maxSize_ = size; }
  void setFileName(const std::string &file) override {
    filename_ = file;
    pathInfo_ = PathInfo(file);
    mkdirAll(pathInfo_.dirPart, 0755);
  }

  ssize_t write(const char *data, size_t len) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (len > maxSize())
      return -1;

    if (fp_ == nullptr)
      open();

    if (curSize_ + len > maxSize())
      rotate();

    size_t n = fwrite(data, sizeof(char), len, fp_);
    fflush(fp_);
    curSize_ += n;
    return n;
  }

  uint64_t maxSize() const {
    if (maxSize_ == 0)
      return kDefaultMaxSize * kBytesPerMB;
    return maxSize_ * kBytesPerMB;
  }

  std::string filename() const {
    if (filename_ != "")
      return filename_;
    return "/tmp/lim.log";
  }

private:
  void open() {
    std::string file = filename();
    if (exist(file))
      return openExist();
    return openNew(false);
  }

  void openExist() {
    std::string file = filename();
    FILE *fp = fopen(file.c_str(), "a+");
    if (fp == nullptr) {
      fprintf(stderr, "fail to open %s: %s", file.c_str(), strerror(errno));
      exit(2);
    }
    fp_ = fp;
    curSize_ = filesize(file);
    fprintf(stderr, "%s - %" PRIu64 "\n", file.c_str(), curSize_);
  }

  void openNew(bool exist) {
    mkdirAll(pathInfo_.dirPart, 0755);

    std::string file = filename();
    std::string bakfile = backupName();
    if (exist)
      rename(file.c_str(), bakfile.c_str());

    FILE *fp = fopen(file.c_str(), "a+");
    if (fp == nullptr) {
      fprintf(stderr, "fail to open %s: %s", file.c_str(), strerror(errno));
      exit(2);
    }

    if (exist)
      copyMode(bakfile, file);
    fp_ = fp;
    curSize_ = 0;
  }

  std::string backupName() {
    char bak[128];
    std::string ts = detail::Timestamp::now().format();
    snprintf(bak, sizeof(bak), "%s/%s%s%s", pathInfo_.dirPart.c_str(),
             pathInfo_.prefix.c_str(), ts.c_str(), pathInfo_.extPart.c_str());
    return bak;
  }

  void rotate() {
    if (::fclose(fp_) != 0)
      fprintf(stderr, "fail to close: %s", strerror(errno));
    openNew(true);
    keepMaxBackups();
  }

  void keepMaxBackups() {
    if (maxBackups_ == 0)
      return;

    std::vector<std::string> files = readDir(pathInfo_.dirPart);
    std::vector<LogFileInfo> oldLogInfos;

    for (const auto &f : files) {
      detail::Timestamp ts =
          timeFromName(f, pathInfo_.prefix, pathInfo_.extPart);
      if (ts.timestamp() != 0)
        oldLogInfos.push_back(LogFileInfo(f, ts));
    }

    std::sort(oldLogInfos.begin(), oldLogInfos.end(),
              [](const LogFileInfo &lhs, const LogFileInfo &rhs) {
                return lhs.ts.compare(rhs.ts) > 0;
              });

    std::vector<LogFileInfo> removeFiles;
    uint32_t i = 0;
    if (maxBackups_ > 0 && maxBackups_ < oldLogInfos.size()) {
      std::vector<LogFileInfo> remain;
      for (const auto &info : oldLogInfos) {
        if (++i >= maxBackups_)
          removeFiles.push_back(info);
        else
          remain.push_back(info);
      }
      oldLogInfos = remain;
    }

    if (maxAge_ > 0) {
      std::vector<LogFileInfo> remain;
      detail::Timestamp cur = detail::Timestamp::now();
      uint64_t maxSec = maxAge_ * 60 * 60 * 24;
      for (const auto &info : oldLogInfos) {
        if (cur.timestamp() < info.ts.timestamp() + maxSec)
          removeFiles.push_back(info);
        else
          remain.push_back(info);
      }
      oldLogInfos = remain;
    }

    for (const auto &info : removeFiles) {
      std::string path = pathInfo_.dirPart + '/' + info.filename;
      remove(path.c_str());
    }

    // TODO, compress remain files.
  }

private:
  std::mutex mutex_;
  std::string filename_;
  uint64_t maxSize_;
  uint32_t maxAge_;
  uint32_t maxBackups_;
  uint64_t curSize_;
  FILE *fp_;
  PathInfo pathInfo_;
};

} // namespace limlog