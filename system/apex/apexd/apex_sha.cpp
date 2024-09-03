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

#include "apex_sha.h"

#include <android-base/logging.h>
#include <openssl/sha.h>

#include <fstream>
#include <sstream>

static constexpr const int kBufSize = 1024;

using android::base::Error;
using android::base::Result;

namespace android {
namespace apex {

Result<std::string> CalculateSha512(const std::string& path) {
  LOG(DEBUG) << "Calculating SHA512 of " << path;
  SHA512_CTX ctx;
  SHA512_Init(&ctx);
  std::ifstream apex(path, std::ios::binary);
  if (apex.bad()) {
    return Error() << "Failed to open " << path;
  }
  char buf[kBufSize];
  while (!apex.eof()) {
    apex.read(buf, kBufSize);
    if (apex.bad()) {
      return Error() << "Failed to read " << path;
    }
    int bytes_read = apex.gcount();
    SHA512_Update(&ctx, buf, bytes_read);
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

Result<std::string> CalculateSha256(const std::string& path) {
  LOG(DEBUG) << "Calculating SHA256 of " << path;
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  std::ifstream apex(path, std::ios::binary);
  if (apex.bad()) {
    return Error() << "Failed to open " << path;
  }
  char buf[kBufSize];
  while (!apex.eof()) {
    apex.read(buf, kBufSize);
    if (apex.bad()) {
      return Error() << "Failed to read " << path;
    }
    int bytes_read = apex.gcount();
    SHA256_Update(&ctx, buf, bytes_read);
  }
  uint8_t hash[SHA256_DIGEST_LENGTH];
  SHA256_Final(hash, &ctx);
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }
  return ss.str();
}

}  // namespace apex
}  // namespace android
