//===- LogFile.h - Log file interface ---------------------------*- C++ -*-===//
//
/// \file
/// Rotate log file by file size.
//
// Author:  zxh
// Date:    2020/08/28 15:22:46
//===----------------------------------------------------------------------===//

#pragma once

#include <filesystem> // C++17
#include <string>
#include <tuple>
#include <utility>

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace limlog {
namespace detail {

class FileHelper {
public:
  explicit FileHelper() = default;
  FileHelper(const FileHelper &) = delete;
  FileHelper &operator=(const FileHelper &) = delete;
  ~FileHelper() { close(); }

  void open(const std::string &filename, bool truncate = false) {
    filename_ = filename;
    const char *mode = truncate ? "wb" : "ab";
    fp_ = fopen(filename_.c_str(), mode);
    if (fp_ == nullptr) {
      fprintf(stderr, "Faild to open file %s for writing, %s\n",
              filename_.c_str(), strerror(errno));
      exit(-1);
    }
  }

  void reopen(bool truncate = false) {
    close();
    open(filename_, truncate);
  }

  void close() {
    if (fp_ != nullptr) {
      fclose(fp_);
      fp_ = nullptr;
    }
  }

  void flush() { fflush(fp_); }

  void write(const char *data, size_t len) {
    if (fwrite(data, sizeof(char), len, fp_) != len)
      fprintf(stderr, "Faild to write file %s, %s\n", filename_.c_str(),
              strerror(errno));
  }

  size_t size() const {
    std::filesystem::path p(filename_);
    return std::filesystem::file_size(p);
  }

private:
  FILE *fp_{nullptr};
  std::string filename_;
};

} // namespace detail

std::tuple<std::string, std::string, std::string>
separate_filename(const std::string &filename) {
  std::filesystem::path p(filename);
  return {p.parent_path(), p.stem(), p.extension()};
}

/// Log to file, auto rotate file by size, and limit file count.
class LogFile {
public:
  LogFile(const std::string &filename, size_t maxSize, size_t maxFile)
      : maxFileSize_(maxSize), maxFileCount_(maxFile) {
    init(filename);
  }

  void write(const char *data, size_t len) {
    currSize_ += len;
    if (currSize_ > maxFileSize_) {
      rotate();
      currSize_ = len;
    }
    fileHelper_.write(data, len);
  }

  void setFileName(const std::string &filename) {
    fileHelper_.close();
    init(filename);
  }

  void setMaxFileSize(size_t nMB) { maxFileSize_ = nMB * kBytesPerMB; }
  void setMaxFileCount(size_t count) { maxFileCount_ = count; }

private:
  void init(const std::string &filename) {
    std::tie(path_, basename_, ext_) = separate_filename(filename);
    createDir();
    std::string fullPath = calcFilename(path_, basename_, ext_, 0);
    fileHelper_.open(fullPath, false);
    currSize_ = fileHelper_.size();
  }

  /// Calculate filename full path.
  std::string calcFilename(const std::string &path, const std::string &basename,
                           const std::string &ext, size_t index) {
    std::string indexStr = index == 0 ? "" : std::to_string(index);

    if (path.empty())
      return basename + indexStr + ext;

    return path + "/" + basename + indexStr + ext;
  }

  /// Create directory is not exists
  void createDir() {
    if (!path_.empty())
      if (!std::filesystem::exists(path_))
        std::filesystem::create_directory(path_);
  }

  /// Rotate log file, file count limit 5.
  /// filename is lastest, e.g.
  ///   filename -> filename.1
  ///   filename.1 -> filename.2
  ///   remove filename.n
  ///   create filename
  void rotate() {
    fileHelper_.close();
    for (auto i = maxFileCount_; i > 0; --i) {
      std::string src = calcFilename(path_, basename_, ext_, i - 1);
      std::string dst = calcFilename(path_, basename_, ext_, i);
      if (std::filesystem::exists(dst))
        std::filesystem::remove(dst);
      if (std::filesystem::exists(src))
        std::filesystem::rename(src, dst);
    }
    fileHelper_.reopen(true);
  }

  detail::FileHelper fileHelper_;
  std::string path_;     // file's parent path, e.g. `/var/log`
  std::string basename_; // file's stem name, e.g. `foo`
  std::string ext_;      // file's extension, e.g. `.log`
  size_t currSize_;      // size of current file
  size_t maxFileSize_;   // max size(byte) of file
  size_t maxFileCount_;  // max count of file
  static const size_t kBytesPerMB = 1024 * 1024;
};

} // namespace limlog