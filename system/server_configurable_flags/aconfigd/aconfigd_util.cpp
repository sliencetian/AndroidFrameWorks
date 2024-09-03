/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <sys/sendfile.h>
#include <dirent.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include "aconfigd_util.h"

using namespace android::base;

namespace android {
namespace aconfigd {

/// Remove all files in a dir
Result<void> RemoveFilesInDir(const std::string& dir) {
  auto dir_ptr = std::unique_ptr<DIR, int (*)(DIR*)>(opendir(dir.c_str()), closedir);
  if (!dir_ptr) {
    return ErrnoError() << "failed to open dir " << dir;
  }

  struct dirent* entry;
  while ((entry = readdir(dir_ptr.get())) != nullptr) {
    if (entry->d_type != DT_REG) {
      continue;
    }
    auto file = dir + "/" + entry->d_name;
    if (unlink(file.c_str()) == -1) {
      return ErrnoError() << "unlink() failed for " << file;
    }
  }

  return {};
}

/// Copy file
Result<void> CopyFile(const std::string& src, const std::string& dst, mode_t mode) {
  android::base::unique_fd src_fd(
      TEMP_FAILURE_RETRY(open(src.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC)));
  if (src_fd == -1) {
    return ErrnoError() << "open() failed for " << src;
  }

  if (FileExists(dst.c_str())) {
    if (chmod(dst.c_str(), 0644) == -1) {
      return ErrnoError() << "chmod() failed for " << dst;
    }
  }

  android::base::unique_fd dst_fd(TEMP_FAILURE_RETRY(
      open(dst.c_str(), O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC | O_CLOEXEC, 0644)));
  if (dst_fd == -1) {
    return ErrnoError() << "open() failed for " << dst;
  }

  struct stat st;
  if (fstat(src_fd.get(), &st) == -1) {
    return ErrnoError() << "fstat() failed";
  }
  auto len = st.st_size;

  if (sendfile(dst_fd, src_fd, nullptr, len) == -1) {
    return ErrnoError() << "sendfile() failed";
  }

  if (chmod(dst.c_str(), mode) == -1) {
    return ErrnoError() << "chmod() failed";
  }

  return {};
}

/// Get a file's timestamp in nano second
Result<uint64_t> GetFileTimeStamp(const std::string& file) {
  struct stat st;
  int result = stat(file.c_str(), &st);
  if (result == -1) {
    return ErrnoError() << "stat() failed";
  }
  uint64_t timestamp = st.st_mtim.tv_sec*1000000000 + st.st_mtim.tv_nsec;
  return timestamp;
}

bool FileExists(const std::string& file) {
  struct stat st;
  return stat(file.c_str(), &st) == 0 ? true : false;
}

bool FileNonZeroSize(const std::string& file) {
  struct stat st;
  return stat(file.c_str(), &st) == 0 ? st.st_size > 0 : false;
}

Result<std::string> GetFilesDigest(const std::vector<std::string>& files) {
  SHA512_CTX ctx;
  SHA512_Init(&ctx);

  for (const auto& file : files) {
    std::ifstream stream(file, std::ios::binary);
    if (stream.bad()) {
      return Error() << "Failed to open " << file;
    }

    char buf[1024];
    while (!stream.eof()) {
      stream.read(buf, 1024);
      if (stream.bad()) {
        return Error() << "Failed to read " << file;
      }
      int bytes_read = stream.gcount();
      SHA512_Update(&ctx, buf, bytes_read);
    }
  }

  uint8_t hash[SHA512_DIGEST_LENGTH];
  SHA512_Final(hash, &ctx);
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }
  return ss.str();
}

} // namespace aconfig
} // namespace android
