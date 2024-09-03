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

#include <string>
#include <sys/stat.h>

#include <android-base/result.h>
#include <android-base/file.h>

#define RETURN_IF_ERROR(RESULT, ERROR) \
if (!RESULT.ok()) {\
  return android::base::Error() << ERROR << ": " << RESULT.error();\
}

namespace android {
  namespace aconfigd {

  /// Remove files in a dir
  base::Result<void> RemoveFilesInDir(const std::string& dir);

  /// Copy file
  base::Result<void> CopyFile(const std::string& src,
                              const std::string& dst,
                              mode_t mode);

  /// Get a file's timestamp in nano second
  base::Result<uint64_t> GetFileTimeStamp(const std::string& file);

  /// Check if file exists
  bool FileExists(const std::string& file);

  /// Check if file exists and has non zero size
  bool FileNonZeroSize(const std::string& file);

  /// Get file digest
  base::Result<std::string> GetFilesDigest(const std::vector<std::string>& files);

  /// Read protobuf from file
  template <typename T>
  base::Result<T> ReadPbFromFile(const std::string& pb_file) {
    auto pb = T();
    if (FileExists(pb_file)) {
      auto content = std::string();
      if (!base::ReadFileToString(pb_file, &content)) {
        return base::ErrnoError() << "ReadFileToString() failed";
      }

      if (!pb.ParseFromString(content)) {
        return base::ErrnoError() << "Unable to parse to protobuf";
      }
    }
    return pb;
  }

  /// Write protobuf to file
  template <typename T>
  base::Result<void> WritePbToFile(const T& pb,
                                   const std::string& file_name,
                                   mode_t mode = 0644) {
    auto content = std::string();
    if (!pb.SerializeToString(&content)) {
      return base::ErrnoError() << "Unable to serialize protobuf to string";
    }

    if (!base::WriteStringToFile(content, file_name)) {
      return base::ErrnoError() << "WriteStringToFile() failed";
    }

    if (chmod(file_name.c_str(), mode) == -1) {
      return base::ErrnoError() << "chmod() failed";
    };

    return {};
  }

  }// namespace aconfig
} // namespace android
