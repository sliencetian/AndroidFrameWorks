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

#include <sys/stat.h>
#include <gtest/gtest.h>
#include <android-base/file.h>
#include <android-base/logging.h>

#include <aconfigd.pb.h>
#include "aconfigd_util.h"
#include "aconfigd.h"

namespace android {
namespace aconfigd {

struct AconfigdMock {
  TemporaryDir root_dir;
  const std::string flags_dir;
  const std::string maps_dir;
  const std::string boot_dir;
  const std::string persist_pb;
  Aconfigd aconfigd;

  AconfigdMock()
      : root_dir()
      , flags_dir(std::string(root_dir.path) + "/flags")
      , maps_dir(std::string(root_dir.path) + "/maps")
      , boot_dir(std::string(root_dir.path) + "/boot")
      , persist_pb(std::string(root_dir.path) + "/persist.pb")
      , aconfigd(root_dir.path, persist_pb) {
    mkdir(flags_dir.c_str(), 0770);
    mkdir(maps_dir.c_str(), 0770);
    mkdir(boot_dir.c_str(), 0775);
  }

  base::Result<StorageReturnMessage> SendRequestToSocket(
      const StorageRequestMessage& request) {
    auto return_msg = StorageReturnMessage();
    auto result = aconfigd.HandleSocketRequest(request, return_msg);
    if (!result.ok()) {
      return base::Error() << result.error();
    } else {
      return return_msg;
    }
  }
};

struct ContainerMock {
  TemporaryDir root_dir;
  const std::string container;
  const std::string package_map;
  const std::string flag_map;
  const std::string flag_val;

  ContainerMock(const std::string& container_name,
                const std::string& package_map_file,
                const std::string& flag_map_file,
                const std::string& flag_val_file)
      : root_dir()
      , container(container_name)
      , package_map(std::string(root_dir.path) + "/etc/aconfig/package.map")
      , flag_map(std::string(root_dir.path) + "/etc/aconfig/flag.map")
      , flag_val(std::string(root_dir.path) + "/etc/aconfig/flag.val") {
    auto etc_dir = std::string(root_dir.path) + "/etc";
    auto aconfig_dir = etc_dir + "/aconfig";
    mkdir(etc_dir.c_str(), 0777);
    mkdir(aconfig_dir.c_str(), 0777);
    CopyFile(package_map_file, package_map, 0444);
    CopyFile(flag_map_file, flag_map, 0444);
    CopyFile(flag_val_file, flag_val, 0444);
  }
};

class AconfigdTest : public ::testing::Test {
 protected:

  StorageRequestMessage new_storage_message(const std::string& container,
                                            const std::string& package_map_file,
                                            const std::string& flag_map_file,
                                            const std::string& flag_value_file) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_new_storage_message();
    msg->set_container(container);
    msg->set_package_map(package_map_file);
    msg->set_flag_map(flag_map_file);
    msg->set_flag_value(flag_value_file);
    return message;
  }

  StorageRequestMessage new_storage_message(const ContainerMock& mock) {
    return new_storage_message(
        mock.container, mock.package_map, mock.flag_map, mock.flag_val);
  }

  StorageRequestMessage flag_override_message(const std::string& package,
                                              const std::string& flag,
                                              const std::string& value,
                                              bool is_local) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_flag_override_message();
    msg->set_package_name(package);
    msg->set_flag_name(flag);
    msg->set_flag_value(value);
    msg->set_is_local(is_local);
    return message;
  }

  StorageRequestMessage flag_query_message(const std::string& package,
                                           const std::string& flag) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_flag_query_message();
    msg->set_package_name(package);
    msg->set_flag_name(flag);
    return message;
  }

  StorageRequestMessage flag_local_override_remove_message(
      const std::string& package,
      const std::string& flag,
      bool remove_all = false) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_remove_local_override_message();
    msg->set_package_name(package);
    msg->set_flag_name(flag);
    msg->set_remove_all(remove_all);
    return message;
  }

  StorageRequestMessage reset_storage_message() {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_reset_storage_message();
    return message;
  }

  StorageRequestMessage list_storage_message() {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_list_storage_message();
    msg->set_all(true);
    return message;
  }

  StorageRequestMessage list_container_storage_message(const std::string& container) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_list_storage_message();
    msg->set_container(container);
    return message;
  }

  StorageRequestMessage list_package_storage_message(const std::string& package) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_list_storage_message();
    msg->set_package_name(package);
    return message;
  }

  StorageRequestMessage ota_flag_staging_message(
      const std::string& build_id,
      const std::vector<std::tuple<std::string, std::string, std::string>> flags) {
    auto message = StorageRequestMessage();
    auto* msg = message.mutable_ota_staging_message();
    msg->set_build_id(build_id);
    for (auto const& [package_name, flag_name, flag_value] : flags) {
      auto* flag = msg->add_overrides();
      flag->set_package_name(package_name);
      flag->set_flag_name(flag_name);
      flag->set_flag_value(flag_value);
    }
    return message;
  }

  void verify_new_storage_return_message(base::Result<StorageReturnMessage> msg_result,
                                         bool ensure_updated = false) {
    ASSERT_TRUE(msg_result.ok()) << msg_result.error();
    auto msg = *msg_result;
    ASSERT_TRUE(msg.has_new_storage_message()) << msg.error_message();
    if (ensure_updated) {
      auto message = msg.new_storage_message();
      ASSERT_TRUE(message.storage_updated());
    }
  }

  void verify_flag_override_return_message(base::Result<StorageReturnMessage> msg_result) {
    ASSERT_TRUE(msg_result.ok()) << msg_result.error();
    auto msg = *msg_result;
    ASSERT_TRUE(msg.has_flag_override_message()) << msg.error_message();
  }

  void verify_flag_query_return_message(
      const StorageReturnMessage::FlagQueryReturnMessage& message,
      const std::string& package_name,
      const std::string& flag_name,
      const std::string& server_value,
      const std::string& local_value,
      const std::string& boot_value,
      const std::string& default_value,
      bool is_readwrite,
      bool has_server_override,
      bool has_local_override) {
    ASSERT_EQ(message.package_name(), package_name);
    ASSERT_EQ(message.flag_name(), flag_name);
    ASSERT_EQ(message.server_flag_value(), server_value);
    ASSERT_EQ(message.local_flag_value(), local_value);
    ASSERT_EQ(message.boot_flag_value(), boot_value);
    ASSERT_EQ(message.default_flag_value(), default_value);
    ASSERT_EQ(message.is_readwrite(), is_readwrite);
    ASSERT_EQ(message.has_server_override(), has_server_override);
    ASSERT_EQ(message.has_local_override(), has_local_override);
  }

  void verify_flag_query_return_message(base::Result<StorageReturnMessage> msg_result,
                                        const std::string& package_name,
                                        const std::string& flag_name,
                                        const std::string& server_value,
                                        const std::string& local_value,
                                        const std::string& boot_value,
                                        const std::string& default_value,
                                        bool is_readwrite,
                                        bool has_server_override,
                                        bool has_local_override) {
    ASSERT_TRUE(msg_result.ok()) << msg_result.error();
    auto msg = *msg_result;
    ASSERT_TRUE(msg.has_flag_query_message()) << msg.error_message();
    auto message = msg.flag_query_message();
    verify_flag_query_return_message(
        message, package_name, flag_name, server_value, local_value, boot_value,
        default_value, is_readwrite, has_server_override, has_local_override);
  }

  void verify_local_override_remove_return_message(
      base::Result<StorageReturnMessage> msg_result) {
    ASSERT_TRUE(msg_result.ok()) << msg_result.error();
    auto msg = *msg_result;
    ASSERT_TRUE(msg.has_remove_local_override_message()) << msg.error_message();
  }

  void verify_reset_storage_message(base::Result<StorageReturnMessage> msg_result) {
    ASSERT_TRUE(msg_result.ok()) << msg_result.error();
    auto msg = *msg_result;
    ASSERT_TRUE(msg.has_reset_storage_message()) << msg.error_message();
  }

  void verify_ota_staging_return_message(base::Result<StorageReturnMessage> msg_result) {
    ASSERT_TRUE(msg_result.ok()) << msg_result.error();
    auto msg = *msg_result;
    ASSERT_TRUE(msg.has_ota_staging_message()) << msg.error_message();
  }

  void verify_error_message(base::Result<StorageReturnMessage> msg_result,
                            const std::string& errmsg) {
    ASSERT_FALSE(msg_result.ok());
    ASSERT_TRUE(msg_result.error().message().find(errmsg) != std::string::npos)
        << msg_result.error().message();
  }

  void verify_equal_file_content(const std::string& file_one,
                                 const std::string& file_two) {

    auto content_one = std::string();
    auto content_two = std::string();
    ASSERT_TRUE(base::ReadFileToString(file_one, &content_one)) << strerror(errno);
    ASSERT_TRUE(base::ReadFileToString(file_two, &content_two)) << strerror(errno);
    ASSERT_EQ(content_one, content_two) << file_one << " is different from "
                                        << file_two;
  }

  // setup test suites
  static void SetUpTestSuite() {
    auto test_dir = base::GetExecutableDirectory();
    package_map_ = test_dir + "/tests/package.map";
    flag_map_ = test_dir + "/tests/flag.map";
    flag_val_ = test_dir + "/tests/flag.val";
    updated_package_map_ = test_dir + "/tests/updated_package.map";
    updated_flag_map_ = test_dir + "/tests/updated_flag.map";
    updated_flag_val_ = test_dir + "/tests/updated_flag.val";
  }

  static std::string package_map_;
  static std::string flag_map_;
  static std::string flag_val_;
  static std::string updated_package_map_;
  static std::string updated_flag_map_;
  static std::string updated_flag_val_;
}; // class AconfigdTest

std::string AconfigdTest::package_map_;
std::string AconfigdTest::flag_map_;
std::string AconfigdTest::flag_val_;
std::string AconfigdTest::updated_package_map_;
std::string AconfigdTest::updated_flag_map_;
std::string AconfigdTest::updated_flag_val_;

TEST_F(AconfigdTest, init_platform_storage_fresh) {
  auto a_mock = AconfigdMock();
  auto init_result = a_mock.aconfigd.InitializePlatformStorage();
  ASSERT_TRUE(init_result.ok()) << init_result.error();

  auto platform_aconfig_dirs = std::vector<std::pair<std::string, std::string>>{
    {"system", "/system/etc/aconfig"},
    {"system_ext", "/system_ext/etc/aconfig"},
    {"vendor", "/vendor/etc/aconfig"},
    {"product", "/product/etc/aconfig"}};

  for (auto const& [container, storage_dir] : platform_aconfig_dirs) {
    auto package_map = std::string(storage_dir) + "/package.map";
    auto flag_map = std::string(storage_dir) + "/flag.map";
    auto flag_val = std::string(storage_dir) + "/flag.val";
    if (!FileNonZeroSize(flag_val)) {
      continue;
    }

    ASSERT_TRUE(FileExists(a_mock.maps_dir + "/" + container + ".package.map"));
    ASSERT_TRUE(FileExists(a_mock.maps_dir + "/" + container + ".flag.map"));
    ASSERT_TRUE(FileExists(a_mock.flags_dir + "/" + container + ".val"));
    ASSERT_TRUE(FileExists(a_mock.flags_dir + "/" + container + ".info"));
    ASSERT_TRUE(FileExists(a_mock.boot_dir + "/" + container + ".val"));
    ASSERT_TRUE(FileExists(a_mock.boot_dir + "/" + container + ".info"));

    verify_equal_file_content(a_mock.maps_dir + "/" + container + ".package.map", package_map);
    verify_equal_file_content(a_mock.maps_dir + "/" + container + ".flag.map", flag_map);
    verify_equal_file_content(a_mock.flags_dir + "/" + container + ".val", flag_val);
    verify_equal_file_content(a_mock.boot_dir + "/" + container + ".val", flag_val);
    verify_equal_file_content(a_mock.flags_dir + "/" + container + ".info",
                              a_mock.boot_dir + "/" + container + ".info");
  }
}

TEST_F(AconfigdTest, init_platform_storage_reboot) {
  auto a_mock = AconfigdMock();
  auto init_result = a_mock.aconfigd.InitializePlatformStorage();
  ASSERT_TRUE(init_result.ok()) << init_result.error();
  auto old_timestamp = GetFileTimeStamp(a_mock.boot_dir + "/system.val");
  ASSERT_TRUE(old_timestamp.ok()) << old_timestamp.error();

  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  init_result = a_mock.aconfigd.InitializePlatformStorage();
  ASSERT_TRUE(init_result.ok()) << init_result.error();
  auto new_timestamp = GetFileTimeStamp(a_mock.boot_dir + "/system.val");
  ASSERT_TRUE(new_timestamp.ok()) << new_timestamp.error();

  // the boot file must be refreshed
  ASSERT_TRUE(*new_timestamp != *old_timestamp);

  auto platform_aconfig_dirs = std::vector<std::pair<std::string, std::string>>{
    {"system", "/system/etc/aconfig"},
    {"system_ext", "/system_ext/etc/aconfig"},
    {"vendor", "/vendor/etc/aconfig"},
    {"product", "/product/etc/aconfig"}};

  for (auto const& [container, storage_dir] : platform_aconfig_dirs) {
    auto package_map = std::string(storage_dir) + "/package.map";
    auto flag_map = std::string(storage_dir) + "/flag.map";
    auto flag_val = std::string(storage_dir) + "/flag.val";
    if (!FileNonZeroSize(flag_val)) {
      continue;
    }

    ASSERT_TRUE(FileExists(a_mock.maps_dir + "/" + container + ".package.map"));
    ASSERT_TRUE(FileExists(a_mock.maps_dir + "/" + container + ".flag.map"));
    ASSERT_TRUE(FileExists(a_mock.flags_dir + "/" + container + ".val"));
    ASSERT_TRUE(FileExists(a_mock.flags_dir + "/" + container + ".info"));
    ASSERT_TRUE(FileExists(a_mock.boot_dir + "/" + container + ".val"));
    ASSERT_TRUE(FileExists(a_mock.boot_dir + "/" + container + ".info"));

    verify_equal_file_content(a_mock.maps_dir + "/" + container + ".package.map", package_map);
    verify_equal_file_content(a_mock.maps_dir + "/" + container + ".flag.map", flag_map);
    verify_equal_file_content(a_mock.flags_dir + "/" + container + ".val", flag_val);
    verify_equal_file_content(a_mock.boot_dir + "/" + container + ".val", flag_val);
    verify_equal_file_content(a_mock.flags_dir + "/" + container + ".info",
                              a_mock.boot_dir + "/" + container + ".info");
  }
}

TEST_F(AconfigdTest, init_mainline_storage_fresh) {
  auto a_mock = AconfigdMock();
  auto init_result = a_mock.aconfigd.InitializeMainlineStorage();
  ASSERT_TRUE(init_result.ok()) << init_result.error();
}

TEST_F(AconfigdTest, add_new_storage) {
  // create mocks
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  // mock a socket request
  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  auto digest = GetFilesDigest({c_mock.package_map, c_mock.flag_map, c_mock.flag_val});
  ASSERT_TRUE(digest.ok());

  // verify the record exists in persist records pb
  auto persist_records_pb = PersistStorageRecords();
  auto content = std::string();
  ASSERT_TRUE(base::ReadFileToString(a_mock.persist_pb, &content))
      << strerror(errno);
  ASSERT_TRUE(persist_records_pb.ParseFromString(content)) << strerror(errno);
  bool found = false;
  for (auto& entry : persist_records_pb.records()) {
    if (entry.container() == "mockup") {
      found = true;
      ASSERT_EQ(entry.version(), 1);
      ASSERT_EQ(entry.package_map(), c_mock.package_map);
      ASSERT_EQ(entry.flag_map(), c_mock.flag_map);
      ASSERT_EQ(entry.flag_val(), c_mock.flag_val);
      ASSERT_EQ(entry.digest(), *digest);
      break;
    }
  }
  ASSERT_TRUE(found);

  // verify persist and boot files
  ASSERT_TRUE(FileExists(a_mock.maps_dir + "/mockup.package.map"));
  ASSERT_TRUE(FileExists(a_mock.maps_dir + "/mockup.flag.map"));
  ASSERT_TRUE(FileExists(a_mock.flags_dir + "/mockup.val"));
  ASSERT_TRUE(FileExists(a_mock.flags_dir + "/mockup.info"));
  ASSERT_TRUE(FileExists(a_mock.boot_dir + "/mockup.val"));
  ASSERT_TRUE(FileExists(a_mock.boot_dir + "/mockup.info"));

  verify_equal_file_content(a_mock.maps_dir + "/mockup.package.map", package_map_);
  verify_equal_file_content(a_mock.maps_dir + "/mockup.flag.map", flag_map_);
  verify_equal_file_content(a_mock.flags_dir + "/mockup.val", flag_val_);
  verify_equal_file_content(a_mock.boot_dir + "/mockup.val", flag_val_);
  verify_equal_file_content(a_mock.flags_dir + "/mockup.info",
                            a_mock.boot_dir + "/mockup.info");
}

TEST_F(AconfigdTest, container_update_in_ota) {
  // create mocks
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  // mock a socket request
  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // cache current boot flag info content
  ASSERT_TRUE(FileExists(a_mock.boot_dir + "/mockup.info"));
  auto boot_flag_info_content = std::string();
  ASSERT_TRUE(base::ReadFileToString(a_mock.boot_dir + "/mockup.info",
                                     &boot_flag_info_content));

  // mock an ota container update
  ASSERT_TRUE(CopyFile(updated_package_map_, c_mock.package_map, 0444).ok());
  ASSERT_TRUE(CopyFile(updated_flag_map_, c_mock.flag_map, 0444).ok());
  ASSERT_TRUE(CopyFile(updated_flag_val_, c_mock.flag_val, 0444).ok());

  // force update
  request_msg = new_storage_message(c_mock);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  auto digest = GetFilesDigest({c_mock.package_map, c_mock.flag_map, c_mock.flag_val});
  ASSERT_TRUE(digest.ok());

  // verify the record exists in persist records pb
  auto persist_records_pb = PersistStorageRecords();
  auto content = std::string();
  ASSERT_TRUE(base::ReadFileToString(a_mock.persist_pb, &content))
      << strerror(errno);
  ASSERT_TRUE(persist_records_pb.ParseFromString(content)) << strerror(errno);
  bool found = false;
  for (auto& entry : persist_records_pb.records()) {
    if (entry.container() == "mockup") {
      found = true;
      ASSERT_EQ(entry.version(), 1);
      ASSERT_EQ(entry.package_map(), c_mock.package_map);
      ASSERT_EQ(entry.flag_map(), c_mock.flag_map);
      ASSERT_EQ(entry.flag_val(), c_mock.flag_val);
      ASSERT_EQ(entry.digest(), *digest);
      break;
    }
  }
  ASSERT_TRUE(found);

  // verify persist and boot files
  ASSERT_TRUE(FileExists(a_mock.maps_dir + "/mockup.package.map"));
  ASSERT_TRUE(FileExists(a_mock.maps_dir + "/mockup.flag.map"));
  ASSERT_TRUE(FileExists(a_mock.flags_dir + "/mockup.val"));
  ASSERT_TRUE(FileExists(a_mock.flags_dir + "/mockup.info"));
  ASSERT_TRUE(FileExists(a_mock.boot_dir + "/mockup.val"));
  ASSERT_TRUE(FileExists(a_mock.boot_dir + "/mockup.info"));

  verify_equal_file_content(a_mock.maps_dir + "/mockup.package.map",
                            updated_package_map_);
  verify_equal_file_content(a_mock.maps_dir + "/mockup.flag.map", updated_flag_map_);
  verify_equal_file_content(a_mock.flags_dir + "/mockup.val", updated_flag_val_);

  // the boot copy should never be updated
  verify_equal_file_content(a_mock.boot_dir + "/mockup.val", flag_val_);
  auto new_boot_flag_info_content = std::string();
  ASSERT_TRUE(base::ReadFileToString(a_mock.boot_dir + "/mockup.info",
                                     &new_boot_flag_info_content));
  ASSERT_EQ(boot_flag_info_content, new_boot_flag_info_content);
}

TEST_F(AconfigdTest, server_override) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "false", "",
      "true", "true", true, true, false);

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "true", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "true", "",
      "true", "true", true, true, false);
}

TEST_F(AconfigdTest, server_override_survive_update) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // create a server override
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "false", "",
      "true", "true", true, true, false);

  // mock an ota container update
  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  ASSERT_TRUE(CopyFile(updated_package_map_, c_mock.package_map, 0444).ok());
  ASSERT_TRUE(CopyFile(updated_flag_map_, c_mock.flag_map, 0444).ok());
  ASSERT_TRUE(CopyFile(updated_flag_val_, c_mock.flag_val, 0444).ok());

  // force update
  request_msg = new_storage_message(c_mock);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // server override should persist
  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "false", "",
      "true", "true", true, true, false);
}

TEST_F(AconfigdTest, local_override) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "", "false",
      "true", "true", true, false, true);

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "true", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "", "true",
      "true", "true", true, false, true);
}

TEST_F(AconfigdTest, local_override_survive_update) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // create a local override
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "", "false",
      "true", "true", true, false, true);

  // mock an ota container update
  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  ASSERT_TRUE(CopyFile(updated_package_map_, c_mock.package_map, 0444).ok());
  ASSERT_TRUE(CopyFile(updated_flag_map_, c_mock.flag_map, 0444).ok());
  ASSERT_TRUE(CopyFile(updated_flag_val_, c_mock.flag_val, 0444).ok());

  // force update
  request_msg = new_storage_message(c_mock);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // local override should persist
  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "", "false",
      "true", "true", true, false, true);
}

TEST_F(AconfigdTest, single_local_override_remove) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // local override enabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // local override disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_2", "disabled_rw", "true", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // remove local override enabled_rw
  request_msg = flag_local_override_remove_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_local_override_remove_return_message(return_msg);

  // enabled_rw local override should be gone
  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "", "",
      "true", "true", true, false, false);

  // disabled_rw local override should still exists
  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_2", "disabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_2", "disabled_rw", "", "true",
      "false", "false", true, false, true);
}

TEST_F(AconfigdTest, readonly_flag_override) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_ro", "false", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "Cannot update read only flag");

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_ro", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "Cannot update read only flag");
}

TEST_F(AconfigdTest, nonexist_flag_override) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = flag_override_message("unknown", "enabled_rw", "false", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "Failed to find owning container");

  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "unknown", "false", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "Flag does not exist");
}

TEST_F(AconfigdTest, nonexist_flag_query) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = flag_query_message("unknown", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "Failed to find owning container");

  request_msg = flag_query_message("com.android.aconfig.storage.test_1", "unknown");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "unknown does not exist");
}

TEST_F(AconfigdTest, storage_reset) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // server override enabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // local override disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_2", "disabled_rw", "true", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // storage reset
  request_msg = reset_storage_message();
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_reset_storage_message(return_msg);

  // enabled_rw server override should be gone
  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_1", "enabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_1", "enabled_rw", "", "",
      "true", "true", true, false, false);

  // disabled_rw local override should be gone
  request_msg = flag_query_message(
      "com.android.aconfig.storage.test_2", "disabled_rw");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_query_return_message(
      return_msg, "com.android.aconfig.storage.test_2", "disabled_rw", "", "",
      "false", "false", true, false, false);
}

TEST_F(AconfigdTest, list_package) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // server override disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "disabled_rw", "true", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // local override enabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "enabled_rw", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // list package
  request_msg = list_package_storage_message("com.android.aconfig.storage.test_1");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  ASSERT_TRUE(return_msg.ok()) << return_msg.error();
  auto flags_msg = return_msg->list_storage_message();
  ASSERT_EQ(flags_msg.flags_size(), 3);
  verify_flag_query_return_message(
      flags_msg.flags(0), "com.android.aconfig.storage.test_1", "disabled_rw",
      "true", "", "false", "false", true, true, false);
  verify_flag_query_return_message(
      flags_msg.flags(1), "com.android.aconfig.storage.test_1", "enabled_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(2), "com.android.aconfig.storage.test_1", "enabled_rw",
      "", "false", "true", "true", true, false, true);
}

TEST_F(AconfigdTest, list_container) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // server override test1.disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "disabled_rw", "true", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // local override test2.disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_2", "disabled_rw", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // list container
  request_msg = list_container_storage_message("mockup");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  ASSERT_TRUE(return_msg.ok()) << return_msg.error();
  auto flags_msg = return_msg->list_storage_message();
  ASSERT_EQ(flags_msg.flags_size(), 8);
  verify_flag_query_return_message(
      flags_msg.flags(0), "com.android.aconfig.storage.test_1", "disabled_rw",
      "true", "", "false", "false", true, true, false);
  verify_flag_query_return_message(
      flags_msg.flags(1), "com.android.aconfig.storage.test_1", "enabled_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(2), "com.android.aconfig.storage.test_1", "enabled_rw",
      "", "", "true", "true", true, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(3), "com.android.aconfig.storage.test_2", "disabled_rw",
      "", "false", "false", "false", true, false, true);
  verify_flag_query_return_message(
      flags_msg.flags(4), "com.android.aconfig.storage.test_2", "enabled_fixed_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(5), "com.android.aconfig.storage.test_2", "enabled_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(6), "com.android.aconfig.storage.test_4", "enabled_fixed_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(7), "com.android.aconfig.storage.test_4", "enabled_rw",
      "", "", "true", "true", true, false, false);
}

TEST_F(AconfigdTest, list_all) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  // server override test1.disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_1", "disabled_rw", "true", false);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // local override test2.disabled_rw
  request_msg = flag_override_message(
      "com.android.aconfig.storage.test_2", "disabled_rw", "false", true);
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_flag_override_return_message(return_msg);

  // list all storage
  request_msg = list_storage_message();
  return_msg = a_mock.SendRequestToSocket(request_msg);
  ASSERT_TRUE(return_msg.ok()) << return_msg.error();
  auto flags_msg = return_msg->list_storage_message();
  ASSERT_EQ(flags_msg.flags_size(), 8);
  verify_flag_query_return_message(
      flags_msg.flags(0), "com.android.aconfig.storage.test_1", "disabled_rw",
      "true", "", "false", "false", true, true, false);
  verify_flag_query_return_message(
      flags_msg.flags(1), "com.android.aconfig.storage.test_1", "enabled_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(2), "com.android.aconfig.storage.test_1", "enabled_rw",
      "", "", "true", "true", true, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(3), "com.android.aconfig.storage.test_2", "disabled_rw",
      "", "false", "false", "false", true, false, true);
  verify_flag_query_return_message(
      flags_msg.flags(4), "com.android.aconfig.storage.test_2", "enabled_fixed_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(5), "com.android.aconfig.storage.test_2", "enabled_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(6), "com.android.aconfig.storage.test_4", "enabled_fixed_ro",
      "", "", "true", "true", false, false, false);
  verify_flag_query_return_message(
      flags_msg.flags(7), "com.android.aconfig.storage.test_4", "enabled_rw",
      "", "", "true", "true", true, false, false);
}

TEST_F(AconfigdTest, list_nonexist_package) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = list_package_storage_message("unknown");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "container not found");
}

TEST_F(AconfigdTest, list_nonexist_container) {
  auto a_mock = AconfigdMock();
  auto c_mock = ContainerMock("mockup", package_map_, flag_map_, flag_val_);

  auto request_msg = new_storage_message(c_mock);
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_new_storage_return_message(return_msg, true);

  request_msg = list_container_storage_message("unknown");
  return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_error_message(return_msg, "Missing storage files object");
}

TEST_F(AconfigdTest, ota_flag_staging) {
  auto a_mock = AconfigdMock();
  auto request_msg = ota_flag_staging_message(
      "mock_build_id",
      {{"package_1", "flag_1", "true"},
       {"package_2", "flag_1", "false"}});
  auto return_msg = a_mock.SendRequestToSocket(request_msg);
  verify_ota_staging_return_message(return_msg);
  ASSERT_TRUE(FileExists(a_mock.flags_dir + "/ota.pb"));
  auto pb = ReadPbFromFile<StorageRequestMessage::OTAFlagStagingMessage>(
      a_mock.flags_dir + "/ota.pb");
  ASSERT_TRUE(pb.ok());
  ASSERT_EQ(pb->build_id(), "mock_build_id");
  auto flags = pb->overrides();
  ASSERT_EQ(flags.size(), 2);
  auto flag = pb->overrides(0);
  ASSERT_EQ(flag.package_name(), "package_1");
  ASSERT_EQ(flag.flag_name(), "flag_1");
  ASSERT_EQ(flag.flag_value(), "true");
  flag = pb->overrides(1);
  ASSERT_EQ(flag.package_name(), "package_2");
  ASSERT_EQ(flag.flag_name(), "flag_1");
  ASSERT_EQ(flag.flag_value(), "false");
}

} // namespace aconfigd
} // namespace android
