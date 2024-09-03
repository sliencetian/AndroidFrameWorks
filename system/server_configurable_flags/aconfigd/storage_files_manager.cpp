
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

#include "aconfigd.h"
#include "aconfigd_util.h"
#include "storage_files_manager.h"

using namespace aconfig_storage;

namespace android {
  namespace aconfigd {

  /// get storage files object for a container
  base::Result<StorageFiles*> StorageFilesManager::GetStorageFiles(
      const std::string& container) {
    if (all_storage_files_.count(container) == 0) {
      return base::Error() << "Missing storage files object for " << container;
    }
    return all_storage_files_[container].get();
  }

  /// create mapped files for a container
  base::Result<StorageFiles*> StorageFilesManager::AddNewStorageFiles(
      const std::string& container,
      const std::string& package_map,
      const std::string& flag_map,
      const std::string& flag_val) {
    if (all_storage_files_.count(container)) {
      return base::Error() << "Storage file object for " << container << " already exists";
    }

    auto result = base::Result<void>({});
    auto storage_files = std::make_unique<StorageFiles>(
          container, package_map, flag_map, flag_val, root_dir_, result);

    if (!result.ok()) {
      return base::Error() << "Failed to create storage file object for " << container
                     << ": " << result.error();
    }

    auto storage_files_ptr = storage_files.get();
    all_storage_files_[container].reset(storage_files.release());
    return storage_files_ptr;
  }

  /// restore storage files object from a storage record pb entry
  base::Result<void> StorageFilesManager::RestoreStorageFiles(
      const PersistStorageRecord& pb) {
    if (all_storage_files_.count(pb.container())) {
      return base::Error() << "Storage file object for " << pb.container()
                     << " already exists";
    }

    all_storage_files_[pb.container()] = std::make_unique<StorageFiles>(pb, root_dir_);
    return {};
  }

  /// update existing storage files object with new storage file set
  base::Result<void> StorageFilesManager::UpdateStorageFiles(
      const std::string& container,
      const std::string& package_map,
      const std::string& flag_map,
      const std::string& flag_val) {
    if (!all_storage_files_.count(container)) {
      return base::Error() << "Failed to update storage files object for " << container
                     << ", it does not exist";
    }

    // backup server and local override
    auto storage_files = GetStorageFiles(container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");
    auto server_overrides = (**storage_files).GetServerFlagValues();
    RETURN_IF_ERROR(server_overrides, "Failed to get existing server overrides");

    auto pb_file = (**storage_files).GetStorageRecord().local_overrides;
    auto local_overrides = ReadPbFromFile<LocalFlagOverrides>(pb_file);
    RETURN_IF_ERROR(local_overrides, "Failed to read local overrides from " + pb_file);

    // clean up existing storage files object and recreate
    (**storage_files).RemoveAllPersistFiles();
    all_storage_files_.erase(container);
    storage_files = AddNewStorageFiles(container, package_map, flag_map, flag_val);
    RETURN_IF_ERROR(storage_files, "Failed to add a new storage object for " + container);

    // reapply local overrides
    auto updated_local_overrides = LocalFlagOverrides();
    for (const auto& entry : local_overrides->overrides()) {
      auto has_flag = (**storage_files).HasFlag(entry.package_name(), entry.flag_name());
      RETURN_IF_ERROR(has_flag, "Failed to check if has flag for " + entry.package_name()
                      + "/" + entry.flag_name());
      if (*has_flag) {
        auto context = (**storage_files).GetPackageFlagContext(
            entry.package_name(), entry.flag_name());
        RETURN_IF_ERROR(context, "Failed to find package flag context for " +
                        entry.package_name() + "/" + entry.flag_name());

        auto update = (**storage_files).SetHasLocalOverride(*context, true);
        RETURN_IF_ERROR(update, "Failed to set flag has local override");

        auto* new_override = updated_local_overrides.add_overrides();
        new_override->set_package_name(entry.package_name());
        new_override->set_flag_name(entry.flag_name());
        new_override->set_flag_value(entry.flag_value());
      }
    }
    auto result = WritePbToFile<LocalFlagOverrides>(updated_local_overrides, pb_file);

    // reapply server overrides
    for (const auto& entry : *server_overrides) {
      auto has_flag = (**storage_files).HasFlag(entry.package_name, entry.flag_name);
      RETURN_IF_ERROR(has_flag, "Failed to check if has flag for " + entry.package_name
                      + "/" + entry.flag_name);
      if (*has_flag) {
        auto context = (**storage_files).GetPackageFlagContext(
            entry.package_name, entry.flag_name);
        RETURN_IF_ERROR(context, "Failed to find package flag context for " +
                        entry.package_name + "/" + entry.flag_name);

        auto update = (**storage_files).SetServerFlagValue(*context, entry.flag_value);
        RETURN_IF_ERROR(update, "Failed to set server flag value");
      }
    }

    return {};
  }

  /// add or update storage file set for a container
  base::Result<bool> StorageFilesManager::AddOrUpdateStorageFiles(
      const std::string& container,
      const std::string& package_map,
      const std::string& flag_map,
      const std::string& flag_val) {
    bool new_container = !HasContainer(container);
    bool update_existing_container = false;
    if (!new_container) {
      auto digest = GetFilesDigest({package_map, flag_map, flag_val});
      RETURN_IF_ERROR(digest, "Failed to get digest for " + container);
      auto storage_files = GetStorageFiles(container);
      RETURN_IF_ERROR(storage_files, "Failed to get storage files object");
      if ((**storage_files).GetStorageRecord().digest != *digest) {
        update_existing_container = true;
      }
    }

    // early return if no update is needed
    if (!(new_container || update_existing_container)) {
      return false;
    }

    if (new_container) {
      auto storage_files = AddNewStorageFiles(
          container, package_map, flag_map, flag_val);
      RETURN_IF_ERROR(storage_files, "Failed to add a new storage object for " + container);
    } else {
      auto storage_files = UpdateStorageFiles(
          container, package_map, flag_map, flag_val);
      RETURN_IF_ERROR(storage_files, "Failed to update storage object for " + container);
    }

    return true;
  }

  /// create boot copy
  base::Result<void> StorageFilesManager::CreateStorageBootCopy(
      const std::string& container) {
    if (!HasContainer(container)) {
      return base::Error() << "Cannot create boot copy without persist copy for " << container;
    }
    auto storage_files = GetStorageFiles(container);
    auto copy_result = (**storage_files).CreateBootStorageFiles();
    RETURN_IF_ERROR(copy_result, "Failed to create boot copies for " + container);
    return {};
  }

  /// reset all storage
  base::Result<void> StorageFilesManager::ResetAllStorage() {
    for (const auto& container : GetAllContainers()) {
      auto storage_files = GetStorageFiles(container);
      RETURN_IF_ERROR(storage_files, "Failed to get storage files object");
      bool available = (**storage_files).HasBootCopy();
      StorageRecord record = (**storage_files).GetStorageRecord();

      (**storage_files).RemoveAllPersistFiles();
      all_storage_files_.erase(container);

      if (available) {
        auto storage_files = AddNewStorageFiles(
            container, record.package_map, record.flag_map, record.flag_val);
        RETURN_IF_ERROR(storage_files, "Failed to add a new storage object for " + container);
      }
    }
    return {};
  }

  /// get container name given flag package name
  base::Result<std::string> StorageFilesManager::GetContainer(
      const std::string& package) {
    if (package_to_container_.count(package)) {
      return package_to_container_[package];
    }

    for (const auto& [container, storage_files] : all_storage_files_) {
      auto has_flag = storage_files->HasPackage(package);
      RETURN_IF_ERROR(has_flag, "Failed to check if has flag");

      if (*has_flag) {
        package_to_container_[package] = container;
        return container;
      }
    }

    return base::Error() << "container not found";
  }

  /// Get all storage records
  std::vector<const StorageRecord*> StorageFilesManager::GetAllStorageRecords() {
    auto all_records = std::vector<const StorageRecord*>();
    for (auto const& [container, files_ptr] : all_storage_files_) {
      all_records.push_back(&(files_ptr->GetStorageRecord()));
    }
    return all_records;
  }

  /// get all containers
  std::vector<std::string> StorageFilesManager::GetAllContainers() {
    auto containers = std::vector<std::string>();
    for (const auto& item : all_storage_files_) {
      containers.push_back(item.first);
    }
    return containers;
  }

  /// write to persist storage records pb file
  base::Result<void> StorageFilesManager::WritePersistStorageRecordsToFile(
      const std::string& file_name) {
    auto records_pb = PersistStorageRecords();
    for (const auto& [container, storage_files] : all_storage_files_) {
      const auto& record = storage_files->GetStorageRecord();
      auto* record_pb = records_pb.add_records();
      record_pb->set_version(record.version);
      record_pb->set_container(record.container);
      record_pb->set_package_map(record.package_map);
      record_pb->set_flag_map(record.flag_map);
      record_pb->set_flag_val(record.flag_val);
      record_pb->set_digest(record.digest);
    }
    return WritePbToFile<PersistStorageRecords>(records_pb, file_name);
  }

  /// apply flag override
  base::Result<void> StorageFilesManager::UpdateFlagValue(
      const std::string& package_name,
      const std::string& flag_name,
      const std::string& flag_value,
      bool is_local_override) {

    auto container = GetContainer(package_name);
    RETURN_IF_ERROR(container, "Failed to find owning container");

    auto storage_files = GetStorageFiles(*container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");

    auto context = (**storage_files).GetPackageFlagContext(package_name, flag_name);
    RETURN_IF_ERROR(context, "Failed to find package flag context");

    if (is_local_override) {
      auto update = (**storage_files).SetLocalFlagValue(*context, flag_value);
      RETURN_IF_ERROR(update, "Failed to set local flag override");
    } else {
      auto update =(**storage_files).SetServerFlagValue(*context, flag_value);
      RETURN_IF_ERROR(update, "Failed to set server flag value");
    }

    return {};
  }

  /// apply ota flags and return remaining ota flags
  base::Result<std::vector<FlagOverride>> StorageFilesManager::ApplyOTAFlagsForContainer(
      const std::string& container,
      const std::vector<FlagOverride>& ota_flags) {
    auto storage_files = GetStorageFiles(container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");

    auto remaining_ota_flags = std::vector<FlagOverride>();
    for (const auto& entry : ota_flags) {
      auto has_flag = (**storage_files).HasPackage(entry.package_name());
      RETURN_IF_ERROR(has_flag, "Failed to check if has flag");
      if (*has_flag) {
        auto result = UpdateFlagValue(entry.package_name(),
                                      entry.flag_name(),
                                      entry.flag_value());
        RETURN_IF_ERROR(result, "Failed to apply staged OTA flag " + entry.package_name()
                        + "/" + entry.flag_name());
      } else {
        remaining_ota_flags.push_back(entry);
      }
    }

    return remaining_ota_flags;
  }

  /// remove all local overrides
  base::Result<void> StorageFilesManager::RemoveAllLocalOverrides() {
    for (const auto& [container, storage_files] : all_storage_files_) {
      auto update = storage_files->RemoveAllLocalFlagValue();
      RETURN_IF_ERROR(update, "Failed to remove local overrides for " + container);
    }
    return {};
  }

  /// remove a local override
  base::Result<void> StorageFilesManager::RemoveFlagLocalOverride(
      const std::string& package,
      const std::string& flag) {
    auto container = GetContainer(package);
    RETURN_IF_ERROR(container, "Failed to find owning container");

    auto storage_files = GetStorageFiles(*container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");

    auto context = (**storage_files).GetPackageFlagContext(package, flag);
    RETURN_IF_ERROR(context, "Failed to find package flag context");

    auto removed = (**storage_files).RemoveLocalFlagValue(*context);
    RETURN_IF_ERROR(removed, "Failed to remove local override");

    return {};
  }

  /// list a flag
  base::Result<StorageFiles::FlagSnapshot> StorageFilesManager::ListFlag(
      const std::string& package,
      const std::string& flag) {
    auto container = GetContainer(package);
    RETURN_IF_ERROR(container, "Failed to find owning container");
    auto storage_files = GetStorageFiles(*container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");

    if ((**storage_files).HasBootCopy()) {
      return (**storage_files).ListFlag(package, flag);
    } else{
      return base::Error() << "Container " << *container << " is currently unavailable";
    }
  }

  /// list flags in a package
  base::Result<std::vector<StorageFiles::FlagSnapshot>>
      StorageFilesManager::ListFlagsInPackage(const std::string& package) {
    auto container = GetContainer(package);
    RETURN_IF_ERROR(container, "Failed to find owning container for " + package);
    auto storage_files = GetStorageFiles(*container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");

    if ((**storage_files).HasBootCopy()) {
      return (**storage_files).ListFlags(package);
    } else{
      return base::Error() << "Container " << *container << " is currently unavailable";
    }
  }

  /// list flags in a container
  base::Result<std::vector<StorageFiles::FlagSnapshot>>
      StorageFilesManager::ListFlagsInContainer(const std::string& container) {
    auto storage_files = GetStorageFiles(container);
    RETURN_IF_ERROR(storage_files, "Failed to get storage files object");

    if ((**storage_files).HasBootCopy()) {
      return (**storage_files).ListFlags();
    } else {
      return base::Error() << "Container " << container << " is currently unavailable";
    }
  }

  /// list all available flags
  base::Result<std::vector<StorageFiles::FlagSnapshot>>
      StorageFilesManager::ListAllAvailableFlags() {
    auto total_flags = std::vector<StorageFiles::FlagSnapshot>();
    for (const auto& [container, storage_files] : all_storage_files_) {
      if (!storage_files->HasBootCopy()) {
        continue;
      }
      auto flags = storage_files->ListFlags();
      RETURN_IF_ERROR(flags, "Failed to list flags in " + container);
      total_flags.reserve(total_flags.size() + flags->size());
      total_flags.insert(total_flags.end(), flags->begin(), flags->end());
    }
    return total_flags;
  }

  } // namespace aconfigd
} // namespace android
