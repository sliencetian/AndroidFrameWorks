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
#include <string>
#include <dirent.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

#include "storage_files_manager.h"
#include "aconfigd_util.h"
#include "aconfigd.h"

using namespace android::base;

namespace android {
namespace aconfigd {

/// Handle a flag override request
Result<void> Aconfigd::HandleFlagOverride(
    const StorageRequestMessage::FlagOverrideMessage& msg,
    StorageReturnMessage& return_msg) {
  auto result = storage_files_manager_->UpdateFlagValue(msg.package_name(),
                                                      msg.flag_name(),
                                                      msg.flag_value(),
                                                      msg.is_local());
  RETURN_IF_ERROR(result, "Failed to set flag override");
  return_msg.mutable_flag_override_message();
  return {};
}

/// Handle ota flag staging request
Result<void> Aconfigd::HandleOTAStaging(
    const StorageRequestMessage::OTAFlagStagingMessage& msg,
    StorageReturnMessage& return_msg) {
  auto ota_flags_pb_file = root_dir_ + "/flags/ota.pb";
  auto result = WritePbToFile<StorageRequestMessage::OTAFlagStagingMessage>(
      msg, ota_flags_pb_file);
  RETURN_IF_ERROR(result, "Failed to stage OTA flags");
  return_msg.mutable_ota_staging_message();
  return {};
}

/// Handle new storage request
Result<void> Aconfigd::HandleNewStorage(
    const StorageRequestMessage::NewStorageMessage& msg,
    StorageReturnMessage& return_msg) {
  auto updated = storage_files_manager_->AddOrUpdateStorageFiles(
      msg.container(), msg.package_map(), msg.flag_map(), msg.flag_value());
  RETURN_IF_ERROR(updated, "Failed to add or update container");

  auto write_result = storage_files_manager_->WritePersistStorageRecordsToFile(
      persist_storage_records_);
  RETURN_IF_ERROR(write_result, "Failed to write to persist storage records");

  auto copy = storage_files_manager_->CreateStorageBootCopy(msg.container());
  RETURN_IF_ERROR(copy, "Failed to make a boot copy for " + msg.container());

  auto result_msg = return_msg.mutable_new_storage_message();
  result_msg->set_storage_updated(*updated);
  return {};
}

/// Handle a flag query request
Result<void> Aconfigd::HandleFlagQuery(
    const StorageRequestMessage::FlagQueryMessage& msg,
    StorageReturnMessage& return_msg) {
  auto snapshot = storage_files_manager_->ListFlag(msg.package_name(), msg.flag_name());
  RETURN_IF_ERROR(snapshot, "Failed query failed");
  auto result_msg = return_msg.mutable_flag_query_message();
  result_msg->set_package_name(snapshot->package_name);
  result_msg->set_flag_name(snapshot->flag_name);
  result_msg->set_server_flag_value(snapshot->server_flag_value);
  result_msg->set_local_flag_value(snapshot->local_flag_value);
  result_msg->set_boot_flag_value(snapshot->boot_flag_value);
  result_msg->set_default_flag_value(snapshot->default_flag_value);
  result_msg->set_has_server_override(snapshot->has_server_override);
  result_msg->set_is_readwrite(snapshot->is_readwrite);
  result_msg->set_has_local_override(snapshot->has_local_override);
  result_msg->set_container(snapshot->container);
  return {};
}

/// Handle override removal request
Result<void> Aconfigd::HandleLocalOverrideRemoval(
    const StorageRequestMessage::RemoveLocalOverrideMessage& msg,
    StorageReturnMessage& return_msg) {
  auto result = Result<void>();
  if (msg.remove_all()) {
    result = storage_files_manager_->RemoveAllLocalOverrides();
  } else {
    result = storage_files_manager_->RemoveFlagLocalOverride(
        msg.package_name(), msg.flag_name());
  }
  RETURN_IF_ERROR(result, "");
  return_msg.mutable_remove_local_override_message();
  return {};
}

/// Handle storage reset
Result<void> Aconfigd::HandleStorageReset(StorageReturnMessage& return_msg) {
  auto result = storage_files_manager_->ResetAllStorage();
  RETURN_IF_ERROR(result, "Failed to reset all storage");

  result = storage_files_manager_->WritePersistStorageRecordsToFile(
      persist_storage_records_);
  RETURN_IF_ERROR(result, "Failed to write persist storage records");

  return_msg.mutable_reset_storage_message();
  return {};
}

/// Handle list storage
Result<void> Aconfigd::HandleListStorage(
    const StorageRequestMessage::ListStorageMessage& msg,
    StorageReturnMessage& return_message) {
  auto flags = Result<std::vector<StorageFiles::FlagSnapshot>>();
  switch (msg.msg_case()) {
    case StorageRequestMessage::ListStorageMessage::kAll: {
      flags = storage_files_manager_->ListAllAvailableFlags();
      break;
    }
    case StorageRequestMessage::ListStorageMessage::kContainer: {
      flags = storage_files_manager_->ListFlagsInContainer(msg.container());
      break;
    }
    case StorageRequestMessage::ListStorageMessage::kPackageName: {
      flags = storage_files_manager_->ListFlagsInPackage(msg.package_name());
      break;
    }
    default:
      return Error() << "Unknown list storage message type from aconfigd socket";
  }
  RETURN_IF_ERROR(flags, "Failed to list flags");

  auto* result_msg = return_message.mutable_list_storage_message();
  for (const auto& flag : *flags) {
    auto* flag_msg = result_msg->add_flags();
    flag_msg->set_package_name(flag.package_name);
    flag_msg->set_flag_name(flag.flag_name);
    flag_msg->set_server_flag_value(flag.server_flag_value);
    flag_msg->set_local_flag_value(flag.local_flag_value);
    flag_msg->set_boot_flag_value(flag.boot_flag_value);
    flag_msg->set_default_flag_value(flag.default_flag_value);
    flag_msg->set_is_readwrite(flag.is_readwrite);
    flag_msg->set_has_server_override(flag.has_server_override);
    flag_msg->set_has_local_override(flag.has_local_override);
    flag_msg->set_container(flag.container);
  }
  return {};
}

/// Read OTA flag overrides to be applied for current build
Result<std::vector<FlagOverride>> Aconfigd::ReadOTAFlagOverridesToApply() {
  auto ota_flags = std::vector<FlagOverride>();
  auto ota_flags_pb_file = root_dir_ + "/flags/ota.pb";
  if (FileExists(ota_flags_pb_file)) {
    auto build_id = GetProperty("ro.build.fingerprint", "");
    auto ota_flags_pb = ReadPbFromFile<StorageRequestMessage::OTAFlagStagingMessage>(
        ota_flags_pb_file);
    RETURN_IF_ERROR(ota_flags_pb, "Failed to read ota flags from pb file");
    if (ota_flags_pb->build_id() == build_id) {
      for (const auto& entry : ota_flags_pb->overrides()) {
        ota_flags.push_back(entry);
      }
    }
  }
  return ota_flags;
}

/// Write remaining OTA flag overrides back to pb file
Result<void> Aconfigd::WriteRemainingOTAOverrides(
    const std::vector<FlagOverride>& ota_flags) {
  auto ota_flags_pb_file = root_dir_ + "/flags/ota.pb";

  if (!ota_flags.empty()) {
    auto ota_flags_pb = StorageRequestMessage::OTAFlagStagingMessage();
    auto build_id = GetProperty("ro.build.fingerprint", "");
    ota_flags_pb.set_build_id(build_id);
    for (auto const& entry : ota_flags) {
      auto* flag = ota_flags_pb.add_overrides();
      flag->set_package_name(entry.package_name());
      flag->set_flag_name(entry.flag_name());
      flag->set_flag_value(entry.flag_value());
    }
    auto result = WritePbToFile<StorageRequestMessage::OTAFlagStagingMessage>(
        ota_flags_pb, ota_flags_pb_file);
    RETURN_IF_ERROR(result, "Failed to write remaining staged OTA flags");
  } else {
    if (FileExists(ota_flags_pb_file)) {
      unlink(ota_flags_pb_file.c_str());
    }
  }

  return {};
}

/// Initialize in memory aconfig storage records
Result<void> Aconfigd::InitializeInMemoryStorageRecords() {
  // remove old records pb
  if (FileExists("/metadata/aconfig/persistent_storage_file_records.pb")) {
    unlink("/metadata/aconfig/persistent_storage_file_records.pb");
  }

  // remove old records pb
  if (FileExists("/metadata/aconfig/persist_storage_file_records.pb")) {
    unlink("/metadata/aconfig/persist_storage_file_records.pb");
  }

  auto records_pb = ReadPbFromFile<PersistStorageRecords>(persist_storage_records_);
  RETURN_IF_ERROR(records_pb, "Unable to read persistent storage records");
  for (const auto& entry : records_pb->records()) {
    storage_files_manager_->RestoreStorageFiles(entry);
  }
  return {};
}

/// Initialize platform RO partition flag storage
Result<void> Aconfigd::InitializePlatformStorage() {
  auto init_result = InitializeInMemoryStorageRecords();
  RETURN_IF_ERROR(init_result, "Failed to init from persist stoage records");

  auto remove_result = RemoveFilesInDir(root_dir_ + "/boot");
  RETURN_IF_ERROR(remove_result, "Failed to clean boot dir");

  auto ota_flags = ReadOTAFlagOverridesToApply();
  RETURN_IF_ERROR(ota_flags, "Failed to get remaining staged OTA flags");
  bool apply_ota_flag = !(ota_flags->empty());

  auto value_files = std::vector<std::pair<std::string, std::string>>{
    {"system", "/system/etc/aconfig"},
    {"system_ext", "/system_ext/etc/aconfig"},
    {"vendor", "/vendor/etc/aconfig"},
    {"product", "/product/etc/aconfig"}};

  for (auto const& [container, storage_dir] : value_files) {
    auto package_file = std::string(storage_dir) + "/package.map";
    auto flag_file = std::string(storage_dir) + "/flag.map";
    auto value_file = std::string(storage_dir) + "/flag.val";

    if (!FileNonZeroSize(value_file)) {
      continue;
    }

    auto updated = storage_files_manager_->AddOrUpdateStorageFiles(
        container, package_file, flag_file, value_file);
    RETURN_IF_ERROR(updated, "Failed to add or update storage for container "
                    + container);

    if (apply_ota_flag) {
      ota_flags = storage_files_manager_->ApplyOTAFlagsForContainer(
          container, *ota_flags);
      RETURN_IF_ERROR(ota_flags, "Failed to apply staged OTA flags");
    }

    auto write_result = storage_files_manager_->WritePersistStorageRecordsToFile(
        persist_storage_records_);
    RETURN_IF_ERROR(write_result, "Failed to write to persist storage records");

    auto copied = storage_files_manager_->CreateStorageBootCopy(container);
    RETURN_IF_ERROR(copied, "Failed to create boot snapshot for container "
                    + container);
  }

  if (apply_ota_flag) {
    auto result = WriteRemainingOTAOverrides(*ota_flags);
    RETURN_IF_ERROR(result, "Failed to write remaining staged OTA flags");
  }

  return {};
}

/// Initialize mainline flag storage
Result<void> Aconfigd::InitializeMainlineStorage() {
  auto init_result = InitializeInMemoryStorageRecords();
  RETURN_IF_ERROR(init_result, "Failed to init from persist stoage records");

  auto ota_flags = ReadOTAFlagOverridesToApply();
  RETURN_IF_ERROR(ota_flags, "Failed to get remaining staged OTA flags");
  bool apply_ota_flag = !(ota_flags->empty());

  auto apex_dir = std::unique_ptr<DIR, int (*)(DIR*)>(opendir("/apex"), closedir);
  if (!apex_dir) {
    return {};
  }

  struct dirent* entry;
  while ((entry = readdir(apex_dir.get())) != nullptr) {
    if (entry->d_type != DT_DIR) continue;

    auto container = std::string(entry->d_name);
    if (container[0] == '.') continue;
    if (container.find('@') != std::string::npos) continue;
    if (container == "sharedlibs") continue;

    auto storage_dir = std::string("/apex/") + container + "/etc";
    auto package_file = std::string(storage_dir) + "/package.map";
    auto flag_file = std::string(storage_dir) + "/flag.map";
    auto value_file = std::string(storage_dir) + "/flag.val";

    if (!FileExists(value_file) || !FileNonZeroSize(value_file)) {
      continue;
    }

    auto updated = storage_files_manager_->AddOrUpdateStorageFiles(
        container, package_file, flag_file, value_file);
    RETURN_IF_ERROR(updated, "Failed to add or update storage for container "
                    + container);

    if (apply_ota_flag) {
      ota_flags = storage_files_manager_->ApplyOTAFlagsForContainer(
          container, *ota_flags);
      RETURN_IF_ERROR(ota_flags, "Failed to apply staged OTA flags");
    }

    auto write_result = storage_files_manager_->WritePersistStorageRecordsToFile(
        persist_storage_records_);
    RETURN_IF_ERROR(write_result, "Failed to write to persist storage records");

    auto copied = storage_files_manager_->CreateStorageBootCopy(container);
    RETURN_IF_ERROR(copied, "Failed to create boot snapshot for container "
                    + container);
  }

  if (apply_ota_flag) {
    auto result = WriteRemainingOTAOverrides(*ota_flags);
    RETURN_IF_ERROR(result, "Failed to write remaining staged OTA flags");
  }

  return {};
}

/// Handle incoming messages to aconfigd socket
Result<void> Aconfigd::HandleSocketRequest(const StorageRequestMessage& message,
                                           StorageReturnMessage& return_message) {
  auto result = Result<void>();

  switch (message.msg_case()) {
    case StorageRequestMessage::kNewStorageMessage: {
      auto msg = message.new_storage_message();
      LOG(INFO) << "received a new storage request for " << msg.container()
                << " with storage files " << msg.package_map() << " "
                << msg.flag_map() << " " << msg.flag_value();
      result = HandleNewStorage(msg, return_message);
      break;
    }
    case StorageRequestMessage::kFlagOverrideMessage: {
      auto msg = message.flag_override_message();
      LOG(INFO) << "received a" << (msg.is_local() ? " local " : " server ")
                << "flag override request for " << msg.package_name() << "/"
                << msg.flag_name() << " to " << msg.flag_value();
      result = HandleFlagOverride(msg, return_message);
      break;
    }
    case StorageRequestMessage::kOtaStagingMessage: {
      auto msg = message.ota_staging_message();
      LOG(INFO) << "received ota flag staging requests for " << msg.build_id();
      result = HandleOTAStaging(msg, return_message);
      break;
    }
    case StorageRequestMessage::kFlagQueryMessage: {
      auto msg = message.flag_query_message();
      LOG(INFO) << "received a flag query request for " << msg.package_name()
                << "/" << msg.flag_name();
      result = HandleFlagQuery(msg, return_message);
      break;
    }
    case StorageRequestMessage::kRemoveLocalOverrideMessage: {
      auto msg = message.remove_local_override_message();
      if (msg.remove_all()) {
        LOG(INFO) << "received a global local override removal request";
      } else {
        LOG(INFO) << "received local override removal request for "
                  << msg.package_name() << "/" << msg.flag_name();
      }
      result = HandleLocalOverrideRemoval(msg, return_message);
      break;
    }
    case StorageRequestMessage::kResetStorageMessage: {
      LOG(INFO) << "received reset storage request";
      result = HandleStorageReset(return_message);
      break;
    }
    case StorageRequestMessage::kListStorageMessage: {
      auto msg = message.list_storage_message();
      LOG(INFO) << "received list storage request";
      result = HandleListStorage(msg, return_message);
      break;
    }
    default:
      result = Error() << "Unknown message type from aconfigd socket";
      break;
  }

  return result;
}

} // namespace aconfigd
} // namespace android
