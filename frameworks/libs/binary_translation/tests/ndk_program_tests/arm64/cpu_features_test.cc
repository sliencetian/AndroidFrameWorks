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

#include <asm/hwcap.h>
#include <cpu-features.h>
#include <sys/auxv.h>
#include <sys/system_properties.h>

#include <fstream>
#include <string>

namespace {

// Test CPU features interfaces from
// https://developer.android.com/ndk/guides/cpu-features
//
// Required features for arm64-v8a from
// https://developer.android.com/ndk/guides/abis
// is Armv8.0 only, which assumes the following features:
// fp asimd aes pmull sha1 sha2
//
// For ndk-translation we check that we get all the values as expected.

class ProcCpuinfoFeatures {
 public:
  ProcCpuinfoFeatures(const char* cpuinfo_path) {
    std::ifstream cpuinfo(cpuinfo_path);
    std::string line;
    while (std::getline(cpuinfo, line)) {
      // Warning: line caption for features is architecture dependent!
      if (line.find("Features") != std::string::npos) {
        features_ = line;
        features_ += " ";  // add feature delimiter at the end.
      }
    }
  }

  bool Empty() const { return features_.empty(); }

  bool Get(const char* name) const {
    // Search for name surrounded by delimiters.
    std::string value = " ";
    value += name;
    value += " ";
    return features_.find(name) != std::string::npos;
  }

 private:
  std::string features_;
};

class BitFeatures {
 public:
  explicit BitFeatures(uint64_t features) : features_(features) {}

  bool Empty() const { return features_ == 0; }

  bool Get(uint64_t bits) const { return (features_ & bits) != 0; }

 private:
  uint64_t features_;
};

bool IsNdkTranslation() {
  char value[PROP_VALUE_MAX];
  __system_property_get("ro.dalvik.vm.native.bridge", value);
  return std::string(value) == "libndk_translation.so";
}

TEST(Arm64CpuFeatures, proc_cpuinfo) {
  ProcCpuinfoFeatures cpuinfo("/proc/cpuinfo");
  // Attention: ART mounts guest cpuinfo in case of native bridge. No one does
  // that in case of standalone executable, so by default we observe the host one.
  if (cpuinfo.Empty()) {
    GTEST_LOG_(INFO) << "/proc/cpuinfo features are empty (arm64 cpuinfo isn't mounted?),"
                     << " trying /etc/cpuinfo.arm64.txt\n";

    cpuinfo = ProcCpuinfoFeatures("/etc/cpuinfo.arm64.txt");

    if (cpuinfo.Empty()) {
      GTEST_SKIP() << "No arm64 cpuinfo found";
    }
  }
  // fp asimd aes pmull sha1 sha2
  EXPECT_TRUE(cpuinfo.Get("fp"));
  EXPECT_TRUE(cpuinfo.Get("asimd"));
  EXPECT_TRUE(cpuinfo.Get("aes"));
  EXPECT_TRUE(cpuinfo.Get("pmull"));

  if (IsNdkTranslation()) {
    EXPECT_TRUE(cpuinfo.Get("crc32"));
  } else {
    EXPECT_TRUE(cpuinfo.Get("sha1"));
    EXPECT_TRUE(cpuinfo.Get("sha2"));
  }
}

TEST(Arm64CpuFeatures, getauxval_AT_HWCAP) {
  BitFeatures hwcap(getauxval(AT_HWCAP));

  EXPECT_TRUE(hwcap.Get(HWCAP_FP));
  EXPECT_TRUE(hwcap.Get(HWCAP_ASIMD));
  EXPECT_TRUE(hwcap.Get(HWCAP_AES));
  EXPECT_TRUE(hwcap.Get(HWCAP_PMULL));

  if (IsNdkTranslation()) {
    EXPECT_TRUE(hwcap.Get(HWCAP_CRC32));
  } else {
    EXPECT_TRUE(hwcap.Get(HWCAP_SHA1));
    EXPECT_TRUE(hwcap.Get(HWCAP_SHA2));
  }
}

TEST(Arm64CpuFeatures, getauxval_AT_HWCAP2) {
  if (!IsNdkTranslation()) {
    return;
  }
  BitFeatures hwcap2(getauxval(AT_HWCAP2));
  ASSERT_TRUE(hwcap2.Empty());
}

TEST(Arm64CpuFeatures, android_getCpuFeatures) {
  AndroidCpuFamily cpu_family = android_getCpuFamily();
  ASSERT_EQ(cpu_family, ANDROID_CPU_FAMILY_ARM64);

  BitFeatures android_cpu_features(android_getCpuFeatures());

  EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_FP));
  EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_ASIMD));
  EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_AES));
  EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_PMULL));

  if (IsNdkTranslation()) {
    EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_CRC32));
  } else {
    EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_SHA1));
    EXPECT_TRUE(android_cpu_features.Get(ANDROID_CPU_ARM64_FEATURE_SHA2));
  }
}

}  // namespace
