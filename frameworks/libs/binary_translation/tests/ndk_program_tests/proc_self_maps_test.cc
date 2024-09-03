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

#include "gtest/gtest.h"

#include <sys/mman.h>
#include <unistd.h>  // sysconf(_SC_PAGESIZE)

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

constexpr bool kExactMapping = true;

template <bool kIsExactMapping = false>
bool IsExecutable(void* ptr, size_t size) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  std::unique_ptr<FILE, decltype(&fclose)> fp(fopen("/proc/self/maps", "re"), fclose);
  if (fp == nullptr) {
    ADD_FAILURE() << "Cannot open /proc/self/maps";
    return false;
  }

  char line[BUFSIZ];
  while (fgets(line, sizeof(line), fp.get()) != nullptr) {
    uintptr_t start;
    uintptr_t end;
    char prot[5];  // sizeof("rwxp")
    if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s", &start, &end, prot) == 3) {
      bool is_match;
      if constexpr (kIsExactMapping) {
        is_match = (addr == start);
        if (is_match) {
          EXPECT_EQ(start + size, end);
        }
      } else {
        is_match = (addr >= start) && (addr < end);
        if (is_match) {
          EXPECT_LE(addr + size, end);
        }
      }
      if (is_match) {
        return prot[2] == 'x';
      }
    }
  }
  ADD_FAILURE() << "Didn't find address " << reinterpret_cast<void*>(addr) << " in /proc/self/maps";
  return false;
}

TEST(ProcSelfMaps, ExecutableFromMmap) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  uint8_t* mapping = reinterpret_cast<uint8_t*>(
      mmap(0, 3 * kPageSize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  ASSERT_NE(mapping, nullptr);

  ASSERT_FALSE(IsExecutable(mapping, 3 * kPageSize));

  void* exec_mapping = mmap(mapping + kPageSize,
                            kPageSize,
                            PROT_READ | PROT_EXEC,
                            MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
  ASSERT_NE(exec_mapping, nullptr);

  ASSERT_FALSE(IsExecutable(mapping, kPageSize));
  // Surrounding mappings can be merged with adjacent mappings. But this one must match exactly.
  ASSERT_TRUE(IsExecutable<kExactMapping>(mapping + kPageSize, kPageSize));
  ASSERT_FALSE(IsExecutable(mapping + 2 * kPageSize, kPageSize));

  ASSERT_EQ(munmap(mapping, 3 * kPageSize), 0);
}

TEST(ProcSelfMaps, ExecutableFromMprotect) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  uint8_t* mapping = reinterpret_cast<uint8_t*>(
      mmap(0, 3 * kPageSize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  ASSERT_NE(mapping, nullptr);

  ASSERT_FALSE(IsExecutable(mapping, 3 * kPageSize));

  ASSERT_EQ(mprotect(mapping + kPageSize, kPageSize, PROT_READ | PROT_EXEC), 0);

  ASSERT_FALSE(IsExecutable(mapping, kPageSize));
  // Surrounding mappings can be merged with adjacent mappings. But this one must match exactly.
  ASSERT_TRUE(IsExecutable<kExactMapping>(mapping + kPageSize, kPageSize));
  ASSERT_FALSE(IsExecutable(mapping + 2 * kPageSize, kPageSize));

  ASSERT_EQ(munmap(mapping, 3 * kPageSize), 0);
}

}  // namespace
