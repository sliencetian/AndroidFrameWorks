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

#include <android-base/logging.h>
#include <unistd.h>

#include <aconfig_storage/aconfig_storage_file.hpp>

#include "aconfigd.h"
#include "aconfigd_util.h"
#include "storage_files.h"

using namespace aconfig_storage;

namespace android {
  namespace aconfigd {

  /// constructor for a new storage file set
  StorageFiles::StorageFiles(const std::string& container,
                             const std::string& package_map,
                             const std::string& flag_map,
                             const std::string& flag_val,
                             const std::string& root_dir,
                             base::Result<void>& status)
      : container_(container)
      , storage_record_()
      , package_map_(nullptr)
      , flag_map_(nullptr)
      , flag_val_(nullptr)
      , boot_flag_val_(nullptr)
      , boot_flag_info_(nullptr)
      , persist_flag_val_(nullptr)
      , persist_flag_info_(nullptr) {
    auto version = get_storage_file_version(flag_val);
    if (!version.ok()) {
      status = base::Error() << "failed to get file version: " << version.error();
      return;
    }

    auto digest = GetFilesDigest({package_map, flag_map, flag_val});
    if (!digest.ok()) {
      status = base::Error() << "failed to get files digest: " << digest.error();
      return;
    }

    storage_record_.version = *version;
    storage_record_.container = container;
    storage_record_.package_map = package_map;
    storage_record_.flag_map = flag_map;
    storage_record_.flag_val = flag_val;
    storage_record_.persist_package_map =
        root_dir + "/maps/" + container + ".package.map";
    storage_record_.persist_flag_map =
        root_dir + "/maps/" + container + ".flag.map";
    storage_record_.persist_flag_val =
        root_dir + "/flags/" + container + ".val";
    storage_record_.persist_flag_info =
        root_dir + "/flags/" + container + ".info";
    storage_record_.local_overrides =
        root_dir + "/flags/" + container + "_local_overrides.pb";
    storage_record_.boot_flag_val =
        root_dir + "/boot/" + container + ".val";
    storage_record_.boot_flag_info =
        root_dir + "/boot/" + container + ".info";
    storage_record_.digest= *digest;

    // copy package map file
    auto copy_result = CopyFile(package_map, storage_record_.persist_package_map, 0444);
    if (!copy_result.ok()) {
      status = base::Error() << "CopyFile failed for " << package_map << ": "
                             << copy_result.error();
      return;
    }

    // copy flag map file
    copy_result = CopyFile(flag_map, storage_record_.persist_flag_map, 0444);
    if (!copy_result.ok()) {
      status = base::Error() << "CopyFile failed for " << flag_map << ": "
                             << copy_result.error();
      return;
    }

    // copy flag value file
    copy_result = CopyFile(flag_val, storage_record_.persist_flag_val, 0644);
    if (!copy_result.ok()) {
      status = base::Error() << "CopyFile failed for " << flag_val << ": "
                             << copy_result.error();
      return;
    }

    // create flag info file
    auto create_result = create_flag_info(
        package_map, flag_map, storage_record_.persist_flag_info);
    if (!create_result.ok()) {
      status = base::Error() << "failed to create flag info file for " << container
                             << create_result.error();
      return;
    }
  }

  /// constructor for existing new storage file set
  StorageFiles::StorageFiles(const PersistStorageRecord& pb,
                             const std::string& root_dir)
      : container_(pb.container())
      , storage_record_()
      , package_map_(nullptr)
      , flag_map_(nullptr)
      , flag_val_(nullptr)
      , boot_flag_val_(nullptr)
      , boot_flag_info_(nullptr)
      , persist_flag_val_(nullptr)
      , persist_flag_info_(nullptr) {
    storage_record_.version = pb.version();
    storage_record_.container = pb.container();
    storage_record_.package_map = pb.package_map();
    storage_record_.flag_map = pb.flag_map();
    storage_record_.flag_val = pb.flag_val();
    storage_record_.persist_package_map =
        root_dir + "/maps/" + pb.container() + ".package.map";
    storage_record_.persist_flag_map =
        root_dir + "/maps/" + pb.container() + ".flag.map";
    storage_record_.persist_flag_val =
        root_dir + "/flags/" + pb.container() + ".val";
    storage_record_.persist_flag_info =
        root_dir + "/flags/" + pb.container() + ".info";
    storage_record_.local_overrides =
        root_dir + "/flags/" + pb.container() + "_local_overrides.pb";
    storage_record_.boot_flag_val =
        root_dir + "/boot/" + pb.container() + ".val";
    storage_record_.boot_flag_info =
        root_dir + "/boot/" + pb.container() + ".info";
    storage_record_.digest = pb.digest();
  }

  /// move constructor
  StorageFiles::StorageFiles(StorageFiles&& rhs) {
    if (this != &rhs) {
      *this = std::move(rhs);
    }
  }

  /// move assignment
  StorageFiles& StorageFiles::operator=(StorageFiles&& rhs) {
    if (this != &rhs) {
      container_ = rhs.container_;
      storage_record_ = std::move(rhs.storage_record_);
      package_map_ = std::move(rhs.package_map_);
      flag_map_ = std::move(rhs.flag_map_);
      flag_val_ = std::move(rhs.flag_val_);
      boot_flag_val_ = std::move(rhs.boot_flag_val_);
      boot_flag_info_ = std::move(rhs.boot_flag_info_);
      persist_flag_val_ = std::move(rhs.persist_flag_val_);
      persist_flag_info_ = std::move(rhs.persist_flag_info_);
    }
    return *this;
  }

  /// get package map
  base::Result<const MappedStorageFile*> StorageFiles::GetPackageMap() {
    if (!package_map_) {
      if (storage_record_.persist_package_map.empty()) {
        return base::Error() << "Missing persist package map file";
      }
      auto package_map = map_storage_file(storage_record_.persist_package_map);
      RETURN_IF_ERROR(package_map, "Failed to map persist package map file for " + container_);
      package_map_.reset(*package_map);
    }
    return package_map_.get();
  }

  /// get flag map
  base::Result<const MappedStorageFile*> StorageFiles::GetFlagMap() {
    if (!flag_map_) {
      if (storage_record_.persist_flag_map.empty()) {
        return base::Error() << "Missing persist flag map file";
      }
      auto flag_map = map_storage_file(storage_record_.persist_flag_map);
      RETURN_IF_ERROR(flag_map, "Failed to map persist flag map file for " + container_);
      flag_map_.reset(*flag_map);
    }
    return flag_map_.get();
  }

  /// get default flag val
  base::Result<const MappedStorageFile*> StorageFiles::GetFlagVal() {
    if (!flag_val_) {
      if (storage_record_.flag_val.empty()) {
        return base::Error() << "Missing flag val file";
      }
      auto flag_val = map_storage_file(storage_record_.flag_val);
      RETURN_IF_ERROR(flag_val, "Failed to map flag val file for " + container_);
      flag_val_.reset(*flag_val);
    }
    return flag_val_.get();
  }

  /// get boot flag val
  base::Result<const MappedStorageFile*> StorageFiles::GetBootFlagVal() {
    if (!boot_flag_val_) {
      if (storage_record_.boot_flag_val.empty()) {
        return base::Error() << "Missing boot flag val file";
      }
      auto flag_val = map_storage_file(storage_record_.boot_flag_val);
      RETURN_IF_ERROR(flag_val, "Failed to map boot flag val file for " + container_);
      boot_flag_val_.reset(*flag_val);
    }
    return boot_flag_val_.get();
  }

  /// get boot flag info
  base::Result<const MappedStorageFile*> StorageFiles::GetBootFlagInfo() {
    if (!boot_flag_info_) {
      if (storage_record_.boot_flag_info.empty()) {
        return base::Error() << "Missing boot flag info file";
      }
      auto flag_info = map_storage_file(storage_record_.boot_flag_info);
      RETURN_IF_ERROR(flag_info, "Failed to map boot flag info file for " + container_);
      boot_flag_info_.reset(*flag_info);
    }
    return boot_flag_info_.get();
  }

  /// get persist flag val
  base::Result<const MutableMappedStorageFile*> StorageFiles::GetPersistFlagVal() {
    if (!persist_flag_val_) {
      if (storage_record_.persist_flag_val.empty()) {
        return base::Error() << "Missing persist flag value file";
      }
      auto flag_val = map_mutable_storage_file(storage_record_.persist_flag_val);
      RETURN_IF_ERROR(flag_val, "Failed to map persist flag val file for " + container_);
      persist_flag_val_.reset(*flag_val);
    }
    return persist_flag_val_.get();
  }

  /// get persist flag info
  base::Result<const MutableMappedStorageFile*> StorageFiles::GetPersistFlagInfo() {
    if (!persist_flag_info_) {
      if (storage_record_.persist_flag_info.empty()) {
        return base::Error() << "Missing persist flag info file";
      }
      auto flag_info = map_mutable_storage_file(storage_record_.persist_flag_info);
      RETURN_IF_ERROR(flag_info, "Failed to map persist flag info file for " + container_);
      persist_flag_info_.reset(*flag_info);
    }
    return persist_flag_info_.get();
  }

  /// check if flag is read only
  base::Result<bool> StorageFiles::IsFlagReadOnly(const PackageFlagContext& context) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_info_file = GetPersistFlagInfo();
    if (!flag_info_file.ok()) {
      return base::Error() << flag_info_file.error();
    }

    auto attribute = get_flag_attribute(
        **flag_info_file, context.value_type, context.flag_index);

    if (!attribute.ok()) {
      return base::Error() << "Failed to get flag attribute";
    }

    return !(*attribute & FlagInfoBit::IsReadWrite);
  }

  /// apply local update to boot flag value copy
  base::Result<void> StorageFiles::ApplyLocalOverrideToBootFlagValue() {
    auto flag_value_result = map_mutable_storage_file(storage_record_.boot_flag_val);
    if (!flag_value_result.ok()) {
      return base::Error() << "Failed to map boot flag value file for local override: "
                           << flag_value_result.error();
    }
    auto flag_value = std::unique_ptr<MutableMappedStorageFile>(*flag_value_result);

    auto pb_file = storage_record_.local_overrides;
    auto pb = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    if (!pb.ok()) {
      return base::Error() << "Failed to read pb from " << pb_file << ": " << pb.error();
    }

    auto applied_overrides = LocalFlagOverrides();
    for (auto& entry : pb->overrides()) {

      // find flag value type and index
      auto context = GetPackageFlagContext(entry.package_name(), entry.flag_name());
      if (!context.ok()) {
        return base::Error() << "Failed to find flag: " << context.error();
      }

      if (!context->flag_exists) {
        continue;
      }

      // apply a local override
      switch (context->value_type) {
        case FlagValueType::Boolean: {
          // validate value
          if (entry.flag_value() != "true" && entry.flag_value() != "false") {
            return base::Error() << "Invalid boolean flag value, it should be true|false";
          }

          // update flag value
          auto update_result = set_boolean_flag_value(
              *flag_value, context->flag_index, entry.flag_value() == "true");
          if (!update_result.ok()) {
            return base::Error() << "Failed to update flag value: " << update_result.error();
          }

          break;
        }
        default:
          return base::Error() << "Unsupported flag value type";
      }

      // mark it applied
      auto new_applied = applied_overrides.add_overrides();
      new_applied->set_package_name(entry.package_name());
      new_applied->set_flag_name(entry.flag_name());
      new_applied->set_flag_value(entry.flag_value());
    }

    if (pb->overrides_size() != applied_overrides.overrides_size()) {
      auto result = WritePbToFile<LocalFlagOverrides>(applied_overrides, pb_file);
      if (!result.ok()) {
        return base::Error() << result.error();
      }
    }

    return {};
  }

  /// has boot copy
  bool StorageFiles::HasBootCopy() {
    return FileExists(storage_record_.boot_flag_val)
        && FileExists(storage_record_.boot_flag_info);
  }

  /// Find flag value type and global index
  base::Result<StorageFiles::PackageFlagContext> StorageFiles::GetPackageFlagContext(
      const std::string& package,
      const std::string& flag) {
    auto result = PackageFlagContext(package, flag);

    // early return
    if (package.empty()) {
      result.package_exists = false;
      result.flag_exists = false;
      return result;
    }

    // find package context
    auto package_map = GetPackageMap();
    if (!package_map.ok()) {
      return base::Error() << package_map.error();
    }

    auto package_context = get_package_read_context(**package_map, package);
    if (!package_context.ok()) {
      return base::Error() << "Failed to get package context for " << package
                           << " in " << container_  << " :" << package_context.error();
    }

    if (!package_context->package_exists) {
      result.flag_exists = false;
      return result;
    } else {
      result.package_exists = true;
    }

    // early return
    if (flag.empty()) {
      return result;
    }

    uint32_t package_id = package_context->package_id;
    uint32_t boolean_flag_start_index = package_context->boolean_start_index;

    // find flag context
    auto flag_map = GetFlagMap();
    if (!flag_map.ok()) {
      return base::Error() << flag_map.error();
    }

    auto flag_context = get_flag_read_context(**flag_map, package_id, flag);
    if (!flag_context.ok()) {
      return base::Error() << "Failed to get flag context of " << package << "/"
                           << flag << " in " << container_  << " :"
                           << flag_context.error();
    }

    if (!flag_context->flag_exists) {
      result.flag_exists = false;
      return result;
    }

    StoredFlagType stored_type = flag_context->flag_type;
    uint16_t within_package_flag_index = flag_context->flag_index;
    auto value_type = map_to_flag_value_type(stored_type);
    if (!value_type.ok()) {
      return base::Error() << "Failed to get flag value type :" << value_type.error();
    }

    result.flag_exists = true;
    result.value_type = *value_type;
    result.flag_index = boolean_flag_start_index + within_package_flag_index;
    return result;
  }

  /// check if has package
  base::Result<bool> StorageFiles::HasPackage(const std::string& package) {
    auto type_and_index = GetPackageFlagContext(package, "");
    if (!type_and_index.ok()) {
      return base::Error() << type_and_index.error();
    }
    return type_and_index->package_exists;
  }

  /// check if has flag
  base::Result<bool> StorageFiles::HasFlag(const std::string& package,
                                           const std::string& flag) {
    auto type_and_index = GetPackageFlagContext(package, flag);
    if (!type_and_index.ok()) {
      return base::Error() << type_and_index.error();
    }
    return type_and_index->flag_exists;
  }

  /// get persistent flag attribute
  base::Result<uint8_t> StorageFiles::GetFlagAttribute(
      const PackageFlagContext& context) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_info_file = GetPersistFlagInfo();
    if (!flag_info_file.ok()) {
      return base::Error() << flag_info_file.error();
    }

    auto attribute = get_flag_attribute(**flag_info_file, context.value_type, context.flag_index);
    if (!attribute.ok()) {
      return base::Error() << "Failed to get flag info: " << attribute.error();
    }

    return *attribute;
  }

  /// get server flag value
  base::Result<std::string> StorageFiles::GetServerFlagValue(
      const PackageFlagContext& context) {
    auto attribute = GetFlagAttribute(context);
    RETURN_IF_ERROR(attribute, "Failed to get flag attribute");

    if (!(*attribute & FlagInfoBit::HasServerOverride)) {
      return std::string();
    }

    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_value_file = GetPersistFlagVal();
    if (!flag_value_file.ok()) {
      return base::Error() << flag_value_file.error();
    }

    switch (context.value_type) {
      case FlagValueType::Boolean: {
        auto value = get_boolean_flag_value(**flag_value_file, context.flag_index);
        if (!value.ok()) {
          return base::Error() << "Failed to get flag value: " << value.error();
        }
        return *value ? "true" : "false";
        break;
      }
      default:
        return base::Error() << "Unsupported flag value type";
    }

    return base::Error() << "Failed to find flag in value file";
  }

  /// get local flag value
  base::Result<std::string> StorageFiles::GetLocalFlagValue(
      const PackageFlagContext& context) {
    auto attribute = GetFlagAttribute(context);
    RETURN_IF_ERROR(attribute, "Failed to get flag attribute");

    if (!(*attribute & FlagInfoBit::HasLocalOverride)) {
      return std::string();
    }

    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto pb_file = storage_record_.local_overrides;
    auto pb = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    if (!pb.ok()) {
      return base::Error() << "Failed to read pb from " << pb_file << ": " << pb.error();
    }

    for (auto& entry : pb->overrides()) {
      if (context.package == entry.package_name()
          && context.flag == entry.flag_name()) {
        return entry.flag_value();
      }
    }

    return base::Error() << "Failed to find flag local override value";
  }

  /// get boot flag value
  base::Result<std::string> StorageFiles::GetBootFlagValue(
      const PackageFlagContext& context) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_value_file = GetBootFlagVal();
    if (!flag_value_file.ok()) {
      return base::Error() << flag_value_file.error();
    }

    switch (context.value_type) {
      case FlagValueType::Boolean: {
        auto value = get_boolean_flag_value(**flag_value_file, context.flag_index);
        if (!value.ok()) {
          return base::Error() << "Failed to get boot flag value: " << value.error();
        }
        return *value ? "true" : "false";
        break;
      }
      default:
        return base::Error() << "Unsupported flag value type";
    }

    return base::Error() << "Failed to find flag in value file";
  }

  /// get default flag value
  base::Result<std::string> StorageFiles::GetDefaultFlagValue(
      const PackageFlagContext& context) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_value_file = GetFlagVal();
    if (!flag_value_file.ok()) {
      return base::Error() << flag_value_file.error();
    }

    switch (context.value_type) {
      case FlagValueType::Boolean: {
        auto value = get_boolean_flag_value(**flag_value_file, context.flag_index);
        if (!value.ok()) {
          return base::Error() << "Failed to get default flag value: " << value.error();
        }
        return *value ? "true" : "false";
        break;
      }
      default:
        return base::Error() << "Unsupported flag value type";
    }

    return base::Error() << "Failed to find flag in value file";
  }

  /// server flag override, update persistent flag value
  base::Result<void> StorageFiles::SetServerFlagValue(const PackageFlagContext& context,
                                                      const std::string& flag_value) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto readonly = IsFlagReadOnly(context);
    RETURN_IF_ERROR(readonly, "Failed to check if flag is readonly");
    if (*readonly) {
      return base::Error() << "Cannot update read only flag";
    }

    auto flag_value_file = GetPersistFlagVal();
    RETURN_IF_ERROR(flag_value_file, "Cannot get persist flag value file");

    switch (context.value_type) {
      case FlagValueType::Boolean: {
        if (flag_value != "true" && flag_value != "false") {
          return base::Error() << "Invalid boolean flag value, it should be true|false";
        }

        auto update = set_boolean_flag_value(
            **flag_value_file, context.flag_index, flag_value == "true");
        RETURN_IF_ERROR(update, "Failed to update flag value");

        update = SetHasServerOverride(context, true);
        RETURN_IF_ERROR(update, "Failed to set flag has server override");

        break;
      }
      default:
        return base::Error() << "Unsupported flag value type";
    }

    return {};
  }

  /// local flag override, update local flag override pb filee
  base::Result<void> StorageFiles::SetLocalFlagValue(const PackageFlagContext& context,
                                                     const std::string& flag_value) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto readonly = IsFlagReadOnly(context);
    RETURN_IF_ERROR(readonly, "Failed to check if flag is readonly")
    if (*readonly) {
      return base::Error() << "Cannot update read only flag";
    }

    auto pb_file = storage_record_.local_overrides;
    auto pb = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    if (!pb.ok()) {
      return base::Error() << "Failed to read pb from " << pb_file << ": " << pb.error();
    }

    bool exist = false;
    for (auto& entry : *(pb->mutable_overrides())) {
      if (entry.package_name() == context.package
          && entry.flag_name() == context.flag) {
        if (entry.flag_value() == flag_value) {
          return {};
        }
        exist = true;
        entry.set_flag_value(flag_value);
        break;
      }
    }

    if (!exist) {
      auto new_override = pb->add_overrides();
      new_override->set_package_name(context.package);
      new_override->set_flag_name(context.flag);
      new_override->set_flag_value(flag_value);
    }

    auto write = WritePbToFile<LocalFlagOverrides>(*pb, pb_file);
    if (!write.ok()) {
      return base::Error() << "Failed to write pb to " << pb_file << ": " << write.error();
    }

    auto update = SetHasLocalOverride(context, true);
    RETURN_IF_ERROR(update, "Failed to set flag has local override");

    return {};
  }

  /// set has server override in flag info
  base::Result<void> StorageFiles::SetHasServerOverride(const PackageFlagContext& context,
                                                        bool has_server_override) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_info_file = GetPersistFlagInfo();
    if (!flag_info_file.ok()) {
      return base::Error() << flag_info_file.error();
    }

    auto update_result = set_flag_has_server_override(
        **flag_info_file, context.value_type, context.flag_index, has_server_override);
    if (!update_result.ok()) {
      return base::Error() << "Failed to update flag has server override: "
                           << update_result.error();
    }

    return {};
  }

  /// set has local override in flag info
  base::Result<void> StorageFiles::SetHasLocalOverride(const PackageFlagContext& context,
                                                       bool has_local_override) {
    if (!context.flag_exists) {
      return base::Error() << "Flag does not exist";
    }

    auto flag_info_file = GetPersistFlagInfo();
    if (!flag_info_file.ok()) {
      return base::Error() << flag_info_file.error();
    }

    auto update_result = set_flag_has_local_override(
        **flag_info_file, context.value_type, context.flag_index, has_local_override);
    if (!update_result.ok()) {
      return base::Error() << "Failed to update flag has local override: "
                           << update_result.error();
    }

    return {};
  }

  /// remove a single flag local override, return if removed
  base::Result<bool> StorageFiles::RemoveLocalFlagValue(
      const PackageFlagContext& context) {

    auto pb_file = storage_record_.local_overrides;
    auto pb = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    if (!pb.ok()) {
      return base::Error() << "Failed to read pb from " << pb_file << ": " << pb.error();
    }

    auto remaining_overrides = LocalFlagOverrides();
    for (auto entry : pb->overrides()) {
      if (entry.package_name() == context.package
          && entry.flag_name() == context.flag) {
        continue;
      }
      auto kept_override = remaining_overrides.add_overrides();
      kept_override->set_package_name(entry.package_name());
      kept_override->set_flag_name(entry.flag_name());
      kept_override->set_flag_value(entry.flag_value());
    }

    if (remaining_overrides.overrides_size() != pb->overrides_size()) {
      auto result = WritePbToFile<LocalFlagOverrides>(remaining_overrides, pb_file);
      if (!result.ok()) {
        return base::Error() << result.error();
      }

      auto update = SetHasLocalOverride(context, false);
      RETURN_IF_ERROR(update, "Failed to unset flag has local override");

      return true;
    } else {
      return false;
    }
  }

  /// remove all local overrides
  base::Result<void> StorageFiles::RemoveAllLocalFlagValue() {
    auto pb_file = storage_record_.local_overrides;
    auto overrides_pb = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    RETURN_IF_ERROR(overrides_pb, "Failed to read local overrides");

    for (auto& entry : overrides_pb->overrides()) {
      auto context = GetPackageFlagContext(entry.package_name(), entry.flag_name());
      RETURN_IF_ERROR(context, "Failed to find package flag context for flag "
                      + entry.package_name() + "/" + entry.flag_name());

      auto update = SetHasLocalOverride(*context, false);
      RETURN_IF_ERROR(update, "Failed to unset flag has local override");
    }

    if (overrides_pb->overrides_size()) {
      auto result = WritePbToFile<LocalFlagOverrides>(
          LocalFlagOverrides(), pb_file);
      RETURN_IF_ERROR(result, "Failed to flush local overrides pb file");
    }

    return {};
  }

  /// get all current server override
  base::Result<std::vector<StorageFiles::ServerOverride>>
      StorageFiles::GetServerFlagValues() {
    auto listed_flags = list_flags_with_info(storage_record_.persist_package_map,
                                             storage_record_.persist_flag_map,
                                             storage_record_.persist_flag_val,
                                             storage_record_.persist_flag_info);
    RETURN_IF_ERROR(
        listed_flags, "Failed to list all flags for " + storage_record_.container);

    auto server_updated_flags = std::vector<ServerOverride>();
    for (const auto& flag : *listed_flags) {
      if (flag.has_server_override) {
        auto server_override = ServerOverride();
        server_override.package_name = std::move(flag.package_name);
        server_override.flag_name = std::move(flag.flag_name);
        server_override.flag_value = std::move(flag.flag_value);
        server_updated_flags.push_back(server_override);
      }
    }

    return server_updated_flags;
  }

  /// remove all storage files
  base::Result<void> StorageFiles::RemoveAllPersistFiles() {
    package_map_.reset(nullptr);
    flag_map_.reset(nullptr);
    flag_val_.reset(nullptr);
    boot_flag_val_.reset(nullptr);
    boot_flag_info_.reset(nullptr);
    persist_flag_val_.reset(nullptr);
    persist_flag_info_.reset(nullptr);
    if (unlink(storage_record_.persist_package_map.c_str()) == -1) {
      return base::ErrnoError() << "unlink() failed for "
                                << storage_record_.persist_package_map;
    }
    if (unlink(storage_record_.persist_flag_map.c_str()) == -1) {
      return base::ErrnoError() << "unlink() failed for "
                                << storage_record_.persist_flag_map;
    }
    if (unlink(storage_record_.persist_flag_val.c_str()) == -1) {
      return base::ErrnoError() << "unlink() failed for "
                                << storage_record_.persist_flag_val;
    }
    if (unlink(storage_record_.persist_flag_info.c_str()) == -1) {
      return base::ErrnoError() << "unlink() failed for "
                                << storage_record_.persist_flag_info;
    }
    if (unlink(storage_record_.local_overrides.c_str()) == -1) {
      return base::ErrnoError() << "unlink() failed for " << storage_record_.local_overrides;
    }
    return {};
  }

  /// create boot flag value and info files
  base::Result<void> StorageFiles::CreateBootStorageFiles() {
    // If the boot copy already exists, do nothing. Never update the boot copy, the boot
    // copy should be boot stable. So in the following scenario: a container storage
    // file boot copy is created, then an updated container is mounted along side existing
    // container. In this case, we should update the persistent storage file copy. But
    // never touch the current boot copy.
    if (FileExists(storage_record_.boot_flag_val)
        && FileExists(storage_record_.boot_flag_info)) {
      return {};
    }

    auto copy = CopyFile(
        storage_record_.persist_flag_val, storage_record_.boot_flag_val, 0444);
    RETURN_IF_ERROR(copy, "CopyFile failed for " + storage_record_.persist_flag_val);

    copy = CopyFile(
        storage_record_.persist_flag_info, storage_record_.boot_flag_info, 0444);
    RETURN_IF_ERROR(copy, "CopyFile failed for " + storage_record_.persist_flag_info);

    // change boot flag value file to 0644 to allow write
    if (chmod(storage_record_.boot_flag_val.c_str(), 0644) == -1) {
      return base::ErrnoError() << "chmod() failed to set to 0644";
    };

    auto apply_result = ApplyLocalOverrideToBootFlagValue();

    // change boot flag value file back to 0444
    if (chmod(storage_record_.boot_flag_val.c_str(), 0444) == -1) {
      if (!apply_result.ok()) {
        return base::ErrnoError() << apply_result.error() << ": "
                                  << "chmod() failed to set to 0444";
      } else {
        return base::ErrnoError() << "chmod() failed to set to 0444";
      }
    };

    return apply_result;
  }

  /// list a flag
  base::Result<StorageFiles::FlagSnapshot> StorageFiles::ListFlag(
      const std::string& package,
      const std::string& flag) {

    auto context = GetPackageFlagContext(package, flag);
    RETURN_IF_ERROR(context, "Failed to find package flag context");

    if (!context->flag_exists) {
      return base::Error() << "Flag " << package << "/" << flag << " does not exist";
    }

    auto attribute = GetFlagAttribute(*context);
    RETURN_IF_ERROR(context, "Failed to get flag attribute");

    auto server_value = GetServerFlagValue(*context);
    RETURN_IF_ERROR(server_value, "Failed to get server flag value");

    auto local_value = GetLocalFlagValue(*context);
    RETURN_IF_ERROR(local_value, "Failed to get local flag value");

    auto boot_value = GetBootFlagValue(*context);
    RETURN_IF_ERROR(boot_value, "Failed to get boot flag value");

    auto default_value = GetDefaultFlagValue(*context);
    RETURN_IF_ERROR(default_value, "Failed to get default flag value");

    auto snapshot = FlagSnapshot();
    snapshot.package_name = package;
    snapshot.flag_name = flag;
    snapshot.default_flag_value = *default_value;
    snapshot.boot_flag_value = *boot_value;
    snapshot.server_flag_value = *server_value;
    snapshot.local_flag_value = *local_value;
    snapshot.is_readwrite = *attribute & FlagInfoBit::IsReadWrite;
    snapshot.has_server_override = *attribute & FlagInfoBit::HasServerOverride;
    snapshot.has_local_override = *attribute & FlagInfoBit::HasLocalOverride;

    return snapshot;
  }

  /// list flags
  base::Result<std::vector<StorageFiles::FlagSnapshot>> StorageFiles::ListFlags(
      const std::string& package) {
    if (!package.empty()) {
      auto has_package = HasPackage(package);
      RETURN_IF_ERROR(
          has_package, package + " does not exist in " + storage_record_.container);
    }

    // fill default value
    auto snapshots = std::vector<FlagSnapshot>();
    auto idxs = std::unordered_map<std::string, size_t>();

    auto listed_flags = list_flags(storage_record_.package_map,
                                   storage_record_.flag_map,
                                   storage_record_.flag_val);
    RETURN_IF_ERROR(
        listed_flags, "Failed to list default flags for " + storage_record_.container);

    for (auto const& flag : *listed_flags) {
      if (package.empty() || package == flag.package_name) {
        idxs[flag.package_name + "/" + flag.flag_name] = snapshots.size();
        snapshots.emplace_back();
        auto& snapshot = snapshots.back();
        snapshot.package_name = std::move(flag.package_name);
        snapshot.flag_name = std::move(flag.flag_name);
        snapshot.default_flag_value = std::move(flag.flag_value);
      }
    }

    // fill boot value
    listed_flags = list_flags(storage_record_.package_map,
                              storage_record_.flag_map,
                              storage_record_.boot_flag_val);
    RETURN_IF_ERROR(
        listed_flags, "Failed to list boot flags for " + storage_record_.container);

    for (auto const& flag : *listed_flags) {
      auto full_flag_name = flag.package_name + "/" + flag.flag_name;
      if (!idxs.count(full_flag_name)) {
        continue;
      }
      auto idx = idxs[full_flag_name];
      snapshots[idx].boot_flag_value = std::move(flag.flag_value);
    }

    // fill server value and attribute
    auto listed_flags_with_info = list_flags_with_info(storage_record_.package_map,
                                                       storage_record_.flag_map,
                                                       storage_record_.persist_flag_val,
                                                       storage_record_.persist_flag_info);
    RETURN_IF_ERROR(listed_flags_with_info,
                    "Failed to list persist flags for " + storage_record_.container);

    for (auto const& flag : *listed_flags_with_info) {
      auto full_flag_name = flag.package_name + "/" + flag.flag_name;
      if (!idxs.count(full_flag_name)) {
        continue;
      }
      auto idx = idxs[full_flag_name];
      if (flag.has_server_override) {
        snapshots[idx].server_flag_value = std::move(flag.flag_value);
      }
      snapshots[idx].is_readwrite = flag.is_readwrite;
      snapshots[idx].has_server_override = flag.has_server_override;
      snapshots[idx].has_local_override = flag.has_local_override;
      snapshots[idx].container = storage_record_.container;
    }

    // fill local value
    auto const& pb_file = storage_record_.local_overrides;
    auto pb = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    RETURN_IF_ERROR(pb, "Failed to read pb from " + pb_file);
    for (const auto& flag : pb->overrides()) {
      auto full_flag_name = flag.package_name() + "/" + flag.flag_name();
      if (!idxs.count(full_flag_name)) {
        continue;
      }
      auto idx = idxs[full_flag_name];
      snapshots[idx].local_flag_value = flag.flag_value();
    }

    auto comp = [](const auto& v1, const auto& v2){
      return (v1.package_name + "/" + v1.flag_name) <
          (v2.package_name + "/" + v2.flag_name);
    };
    std::sort(snapshots.begin(), snapshots.end(), comp);

    return snapshots;
  }

  } // namespace aconfigd
} // namespace android
