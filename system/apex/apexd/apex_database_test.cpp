/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "apex_database.h"

#include <android-base/macros.h>
#include <android-base/result-gmock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <tuple>

using android::base::Error;
using android::base::Result;
using android::base::testing::HasError;
using android::base::testing::Ok;
using android::base::testing::WithMessage;

namespace android {
namespace apex {
namespace {

using MountedApexData = MountedApexDatabase::MountedApexData;

TEST(MountedApexDataTest, LinearOrder) {
  constexpr const char* kLoopName[] = {"loop1", "loop2", "loop3"};
  constexpr const char* kPath[] = {"path1", "path2", "path3"};
  constexpr const char* kMount[] = {"mount1", "mount2", "mount3"};
  constexpr const char* kDm[] = {"dm1", "dm2", "dm3"};
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  constexpr size_t kCount = arraysize(kLoopName) * arraysize(kPath) *
                            arraysize(kMount) * arraysize(kDm);

  auto index_fn = [&](size_t i) {
    const size_t loop_index = i % arraysize(kLoopName);
    const size_t loop_rest = i / arraysize(kLoopName);
    const size_t path_index = loop_rest % arraysize(kPath);
    const size_t path_rest = loop_rest / arraysize(kPath);
    const size_t mount_index = path_rest % arraysize(kMount);
    const size_t mount_rest = path_rest / arraysize(kMount);
    const size_t dm_index = mount_rest % arraysize(kDm);
    CHECK_EQ(mount_rest / arraysize(kDm), 0u);
    return std::make_tuple(loop_index, path_index, mount_index, dm_index);
  };

  MountedApexData data[kCount];
  for (size_t i = 0; i < kCount; ++i) {
    size_t loop_idx, path_idx, mount_idx, dm_idx;
    std::tie(loop_idx, path_idx, mount_idx, dm_idx) = index_fn(i);
    data[i] = MountedApexData(0, kLoopName[loop_idx], kPath[path_idx],
                              kMount[mount_idx], kDm[dm_idx]);
  }

  for (size_t i = 0; i < kCount; ++i) {
    size_t loop_idx_i, path_idx_i, mount_idx_i, dm_idx_i;
    std::tie(loop_idx_i, path_idx_i, mount_idx_i, dm_idx_i) = index_fn(i);
    for (size_t j = i; j < kCount; ++j) {
      size_t loop_idx_j, path_idx_j, mount_idx_j, dm_idx_j;
      std::tie(loop_idx_j, path_idx_j, mount_idx_j, dm_idx_j) = index_fn(j);
      if (loop_idx_i != loop_idx_j) {
        EXPECT_EQ(loop_idx_i < loop_idx_j, data[i] < data[j]);
        continue;
      }
      if (path_idx_i != path_idx_j) {
        EXPECT_EQ(path_idx_i < path_idx_j, data[i] < data[j]);
        continue;
      }
      if (mount_idx_i != mount_idx_j) {
        EXPECT_EQ(mount_idx_i < mount_idx_j, data[i] < data[j]);
        continue;
      }
      if (dm_idx_i != dm_idx_j) {
        EXPECT_EQ(dm_idx_i < dm_idx_j, data[i] < data[j]);
        continue;
      }
    }
  }
}

size_t CountPackages(const MountedApexDatabase& db) {
  size_t ret = 0;
  db.ForallMountedApexes([&ret](const std::string& a ATTRIBUTE_UNUSED,
                                const MountedApexData& b ATTRIBUTE_UNUSED,
                                bool c ATTRIBUTE_UNUSED) { ++ret; });
  return ret;
}

bool Contains(const MountedApexDatabase& db, const std::string& package,
              const std::string& loop_name, const std::string& full_path,
              const std::string& mount_point, const std::string& device_name) {
  bool found = false;
  db.ForallMountedApexes([&](const std::string& p, const MountedApexData& d,
                             bool b ATTRIBUTE_UNUSED) {
    if (package == p && loop_name == d.loop_name && full_path == d.full_path &&
        mount_point == d.mount_point && device_name == d.device_name) {
      found = true;
    }
  });
  return found;
}

bool ContainsPackage(const MountedApexDatabase& db, const std::string& package,
                     const std::string& loop_name, const std::string& full_path,
                     const std::string& dm) {
  bool found = false;
  db.ForallMountedApexes(
      package, [&](const MountedApexData& d, bool b ATTRIBUTE_UNUSED) {
        if (loop_name == d.loop_name && full_path == d.full_path &&
            dm == d.device_name) {
          found = true;
        }
      });
  return found;
}

TEST(ApexDatabaseTest, AddRemovedMountedApex) {
  constexpr const char* kPackage = "package";
  constexpr const char* kLoopName = "loop";
  constexpr const char* kPath = "path";
  constexpr const char* kMountPoint = "mount";
  constexpr const char* kDeviceName = "dev";

  MountedApexDatabase db;
  ASSERT_EQ(CountPackages(db), 0u);

  db.AddMountedApex(kPackage, 0, kLoopName, kPath, kMountPoint, kDeviceName);
  ASSERT_TRUE(
      Contains(db, kPackage, kLoopName, kPath, kMountPoint, kDeviceName));
  ASSERT_TRUE(ContainsPackage(db, kPackage, kLoopName, kPath, kDeviceName));

  db.RemoveMountedApex(kPackage, kPath);
  EXPECT_FALSE(
      Contains(db, kPackage, kLoopName, kPath, kMountPoint, kDeviceName));
  EXPECT_FALSE(ContainsPackage(db, kPackage, kLoopName, kPath, kDeviceName));
}

TEST(ApexDatabaseTest, MountMultiple) {
  constexpr const char* kPackage[] = {"package", "package", "package",
                                      "package"};
  constexpr const char* kLoopName[] = {"loop", "loop2", "loop3", "loop4"};
  constexpr const char* kPath[] = {"path", "path2", "path", "path4"};
  constexpr const char* kMountPoint[] = {"mount", "mount2", "mount", "mount4"};
  constexpr const char* kDeviceName[] = {"dev", "dev2", "dev3", "dev4"};

  MountedApexDatabase db;
  ASSERT_EQ(CountPackages(db), 0u);

  for (size_t i = 0; i < arraysize(kPackage); ++i) {
    db.AddMountedApex(kPackage[i], 0, kLoopName[i], kPath[i], kMountPoint[i],
                      kDeviceName[i]);
  }

  ASSERT_EQ(CountPackages(db), 4u);
  for (size_t i = 0; i < arraysize(kPackage); ++i) {
    ASSERT_TRUE(Contains(db, kPackage[i], kLoopName[i], kPath[i],
                         kMountPoint[i], kDeviceName[i]));
    ASSERT_TRUE(ContainsPackage(db, kPackage[i], kLoopName[i], kPath[i],
                                kDeviceName[i]));
  }

  db.RemoveMountedApex(kPackage[0], kPath[0]);
  EXPECT_FALSE(Contains(db, kPackage[0], kLoopName[0], kPath[0], kMountPoint[0],
                        kDeviceName[0]));
  EXPECT_FALSE(
      ContainsPackage(db, kPackage[0], kLoopName[0], kPath[0], kDeviceName[0]));
  EXPECT_TRUE(Contains(db, kPackage[1], kLoopName[1], kPath[1], kMountPoint[1],
                       kDeviceName[1]));
  EXPECT_TRUE(
      ContainsPackage(db, kPackage[1], kLoopName[1], kPath[1], kDeviceName[1]));
  EXPECT_TRUE(Contains(db, kPackage[2], kLoopName[2], kPath[2], kMountPoint[2],
                       kDeviceName[2]));
  EXPECT_TRUE(
      ContainsPackage(db, kPackage[2], kLoopName[2], kPath[2], kDeviceName[2]));
  EXPECT_TRUE(Contains(db, kPackage[3], kLoopName[3], kPath[3], kMountPoint[3],
                       kDeviceName[3]));
  EXPECT_TRUE(
      ContainsPackage(db, kPackage[3], kLoopName[3], kPath[3], kDeviceName[3]));
}

TEST(ApexDatabaseTest, DoIfLatest) {
  // Check by passing error-returning handler
  // When handler is triggered, DoIfLatest() returns the expected error.
  auto returnError = []() -> Result<void> { return Error() << "expected"; };

  MountedApexDatabase db;

  // With apex: [{version=0,path=path}]
  db.AddMountedApex("package", 0, "loop", "path", "mount", "dev");
  ASSERT_THAT(db.DoIfLatest("package", "path", returnError),
              HasError(WithMessage("expected")));

  // With apexes: [{version=0,path=path}, {version=5,path=path5}]
  db.AddMountedApex("package", 5, "loop5", "path5", "mount5", "dev5");
  ASSERT_THAT(db.DoIfLatest("package", "path", returnError), Ok());
  ASSERT_THAT(db.DoIfLatest("package", "path5", returnError),
              HasError(WithMessage("expected")));
}

TEST(ApexDatabaseTest, GetLatestMountedApex) {
  constexpr const char* kPackage = "package";
  constexpr const char* kLoopName = "loop";
  constexpr const char* kPath = "path";
  constexpr const char* kMountPoint = "mount";
  constexpr const char* kDeviceName = "dev";

  MountedApexDatabase db;
  ASSERT_EQ(CountPackages(db), 0u);

  db.AddMountedApex(kPackage, 0, kLoopName, kPath, kMountPoint, kDeviceName);

  auto ret = db.GetLatestMountedApex(kPackage);
  MountedApexData expected(0, kLoopName, kPath, kMountPoint, kDeviceName);
  ASSERT_TRUE(ret.has_value());
  ASSERT_EQ(ret->loop_name, std::string(kLoopName));
  ASSERT_EQ(ret->full_path, std::string(kPath));
  ASSERT_EQ(ret->mount_point, std::string(kMountPoint));
  ASSERT_EQ(ret->device_name, std::string(kDeviceName));
}

TEST(ApexDatabaseTest, GetLatestMountedApexReturnsNullopt) {
  MountedApexDatabase db;
  auto ret = db.GetLatestMountedApex("no-such-name");
  ASSERT_FALSE(ret.has_value());
}

#pragma clang diagnostic push
// error: 'ReturnSentinel' was marked unused but was used
// [-Werror,-Wused-but-marked-unused]
#pragma clang diagnostic ignored "-Wused-but-marked-unused"

TEST(MountedApexDataTest, NoDuplicateLoopDataLoopDevices) {
  ASSERT_DEATH(
      {
        MountedApexDatabase db;
        db.AddMountedApex("package", 0, "loop", "path", "mount", "dm");
        db.AddMountedApex("package2", 0, "loop", "path2", "mount2", "dm2");
      },
      "Duplicate loop device: loop");
}

TEST(MountedApexDataTest, NoDuplicateDm) {
  ASSERT_DEATH(
      {
        MountedApexDatabase db;
        db.AddMountedApex("package", 0, "loop", "path", "mount", "dm");
        db.AddMountedApex("package2", 0, "loop2", "path2", "mount2", "dm");
      },
      "Duplicate dm device: dm");
}

#pragma clang diagnostic pop

}  // namespace
}  // namespace apex
}  // namespace android
