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

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include <aconfigd.pb.h>
#include "storage_files.h"

namespace android {
  namespace aconfigd {
    /// Manager all storage files across different containers
    class StorageFilesManager {
      public:

      /// constructor
      StorageFilesManager(const std::string& root_dir)
          : root_dir_(root_dir)
          , all_storage_files_()
          , package_to_container_()
      {}

      /// destructor
      ~StorageFilesManager() = default;

      /// no copy
      StorageFilesManager(const StorageFilesManager&) = delete;
      StorageFilesManager& operator=(const StorageFilesManager&) = delete;

      /// move constructor and assignment
      StorageFilesManager(StorageFilesManager&& rhs)
          : root_dir_(rhs.root_dir_)
          , all_storage_files_()
          , package_to_container_() {
        if (this != &rhs) {
          all_storage_files_ = std::move(rhs.all_storage_files_);
          package_to_container_ = std::move(rhs.package_to_container_);
        }
      }
      StorageFilesManager& operator=(StorageFilesManager&& rhs) = delete;

      /// has container
      bool HasContainer(const std::string& container) {
        return all_storage_files_.count(container);
      }

      /// get mapped files for a container
      base::Result<StorageFiles*> GetStorageFiles(const std::string& container);

      /// create mapped files for a container
      base::Result<StorageFiles*> AddNewStorageFiles(const std::string& container,
                                                     const std::string& package_map,
                                                     const std::string& flag_map,
                                                     const std::string& flag_val);

      /// restore storage files object from a storage record pb entry
      base::Result<void> RestoreStorageFiles(const PersistStorageRecord& pb);

      /// update existing storage files object with new storage file set
      base::Result<void> UpdateStorageFiles(const std::string& container,
                                            const std::string& package_map,
                                            const std::string& flag_map,
                                            const std::string& flag_val);

      /// add or update storage file set for a container
      base::Result<bool> AddOrUpdateStorageFiles(const std::string& container,
                                                 const std::string& package_map,
                                                 const std::string& flag_map,
                                                 const std::string& flag_val);

      /// create boot copy
      base::Result<void> CreateStorageBootCopy(const std::string& container);

      /// reset all storage
      base::Result<void> ResetAllStorage();

      /// get container name given flag package name
      base::Result<std::string> GetContainer(const std::string& package);

      /// get all storage records
      std::vector<const StorageRecord*> GetAllStorageRecords();

      /// get all containers
      std::vector<std::string> GetAllContainers();

      /// write to persist storage records pb file
      base::Result<void> WritePersistStorageRecordsToFile(
          const std::string& file_name);

      /// apply flag override
      base::Result<void> UpdateFlagValue(const std::string& package_name,
                                         const std::string& flag_name,
                                         const std::string& flag_value,
                                         bool is_local_override = false);

      /// apply ota flags and return remaining ota flags
      base::Result<std::vector<FlagOverride>> ApplyOTAFlagsForContainer(
          const std::string& container,
          const std::vector<FlagOverride>& ota_flags);

      /// remove all local overrides
      base::Result<void> RemoveAllLocalOverrides();

      /// remove a local override
      base::Result<void> RemoveFlagLocalOverride(const std::string& package,
                                                 const std::string& flag);

      /// list a flag
      base::Result<StorageFiles::FlagSnapshot> ListFlag(const std::string& package,
                                                        const std::string& flag);

      /// list flags in a package
      base::Result<std::vector<StorageFiles::FlagSnapshot>> ListFlagsInPackage(
          const std::string& package);

      /// list flags in a containers
      base::Result<std::vector<StorageFiles::FlagSnapshot>> ListFlagsInContainer(
          const std::string& container);

      /// list all available flags
      base::Result<std::vector<StorageFiles::FlagSnapshot>> ListAllAvailableFlags();

      private:

      /// root directory to store storage files
      const std::string root_dir_;

      /// a hash table from container name to mapped files
      std::unordered_map<std::string, std::unique_ptr<StorageFiles>> all_storage_files_;

      /// a hash table from package name to container name
      std::unordered_map<std::string, std::string> package_to_container_;

    }; // class StorageFilesManager

  } // namespace aconfigd
} // namespace android
