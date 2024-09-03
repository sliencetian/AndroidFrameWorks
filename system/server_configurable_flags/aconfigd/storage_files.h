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

#include <string>

#include <android-base/result.h>

#include <aconfig_storage/aconfig_storage_read_api.hpp>
#include <aconfig_storage/aconfig_storage_write_api.hpp>
#include <aconfigd.pb.h>

namespace android {
  namespace aconfigd {

    /// In memory data structure for storage file locations for each container
    struct StorageRecord {
      int version;
      std::string container;            // container name
      std::string package_map;          // package.map on container
      std::string flag_map;             // flag.map on container
      std::string flag_val;             // flag.val on container
      std::string persist_package_map;  // persist package.map (backup copy for OTA)
      std::string persist_flag_map;     // persist flag.map (backup copy for OTA)
      std::string persist_flag_val;     // persist flag.val
      std::string persist_flag_info;    // persist flag.info
      std::string local_overrides;      // local flag overrides pb file
      std::string boot_flag_val;        // boot flag.val
      std::string boot_flag_info;       // boot flag.info
      std::string digest;               // digest of storage files
    };

    /// Mapped files for a container
    class StorageFiles {
      public:

      /// constructor for a new storage file set
      StorageFiles(const std::string& container,
                   const std::string& package_map,
                   const std::string& flag_map,
                   const std::string& flag_val,
                   const std::string& root_dir,
                   base::Result<void>& status);

      /// constructor for existing new storage file set
      StorageFiles(const PersistStorageRecord& pb,
                   const std::string& root_dir);

      /// destructor
      ~StorageFiles() = default;

      /// no copy
      StorageFiles(const StorageFiles&) = delete;
      StorageFiles& operator=(const StorageFiles&) = delete;

      /// move constructor and assignment
      StorageFiles(StorageFiles&& rhs);
      StorageFiles& operator=(StorageFiles&& rhs);

      /// get storage record
      const StorageRecord& GetStorageRecord() {
        return storage_record_;
      }

      /// has boot copy
      bool HasBootCopy();

      /// return result for package and flag context
      struct PackageFlagContext {
        const std::string& package;
        const std::string& flag;
        bool package_exists;
        bool flag_exists;
        aconfig_storage::FlagValueType value_type;
        uint32_t flag_index;

        PackageFlagContext(const std::string& package_name,
                           const std::string& flag_name)
            : package(package_name)
            , flag(flag_name)
            , package_exists(false)
            , flag_exists(false)
            , value_type()
            , flag_index()
        {}
      };

      /// Find package and flag context
      base::Result<PackageFlagContext> GetPackageFlagContext(
          const std::string& package, const std::string& flag);

      /// check if has package
      base::Result<bool> HasPackage(const std::string& package);

      /// check if has flag
      base::Result<bool> HasFlag(const std::string& package,
                                 const std::string& flag);

      /// get persistent flag attribute
      base::Result<uint8_t> GetFlagAttribute(const PackageFlagContext& context);

      /// get server flag value
      base::Result<std::string> GetServerFlagValue(const PackageFlagContext& context);

      /// get local flag value
      base::Result<std::string> GetLocalFlagValue(const PackageFlagContext& context);

      /// get boot flag value
      base::Result<std::string> GetBootFlagValue(const PackageFlagContext& context);

      /// get default flag value
      base::Result<std::string> GetDefaultFlagValue(const PackageFlagContext& context);

      /// server flag override, update persistent flag value
      base::Result<void> SetServerFlagValue(const PackageFlagContext& context,
                                            const std::string& flag_value);

      /// local flag override, update local flag override pb filee
      base::Result<void> SetLocalFlagValue(const PackageFlagContext& context,
                                           const std::string& flag_value);

      /// set has server override in flag info
      base::Result<void> SetHasServerOverride(const PackageFlagContext& context,
                                              bool has_server_override);

      /// set has local override in flag info
      base::Result<void> SetHasLocalOverride(const PackageFlagContext& context,
                                             bool has_local_override);

      /// remove a single flag local override, return if removed
      base::Result<bool> RemoveLocalFlagValue(const PackageFlagContext& context);

      /// remove all local overrides
      base::Result<void> RemoveAllLocalFlagValue();

      /// strcut for server flag value entries
      struct ServerOverride {
        std::string package_name;
        std::string flag_name;
        std::string flag_value;
      };

      /// get all current server override
      base::Result<std::vector<ServerOverride>> GetServerFlagValues();

      /// remove all persist storage files
      base::Result<void> RemoveAllPersistFiles();

      /// create boot flag value and info files
      base::Result<void> CreateBootStorageFiles();

      /// struct for flag snapshot
      struct FlagSnapshot {
        std::string package_name;
        std::string flag_name;
        std::string server_flag_value;
        std::string local_flag_value;
        std::string boot_flag_value;
        std::string default_flag_value;
        std::string container;
        bool is_readwrite;
        bool has_server_override;
        bool has_local_override;
      };

      /// list a flag
      base::Result<StorageFiles::FlagSnapshot> ListFlag(const std::string& package,
                                                        const std::string& flag);

      /// list flags
      base::Result<std::vector<FlagSnapshot>> ListFlags(
          const std::string& package = "");

      private:

      /// get package map
      base::Result<const aconfig_storage::MappedStorageFile*> GetPackageMap();

      /// get flag map
      base::Result<const aconfig_storage::MappedStorageFile*> GetFlagMap();

      /// get default flag val
      base::Result<const aconfig_storage::MappedStorageFile*> GetFlagVal();

      /// get boot flag val
      base::Result<const aconfig_storage::MappedStorageFile*> GetBootFlagVal();

      /// get boot flag info
      base::Result<const aconfig_storage::MappedStorageFile*> GetBootFlagInfo();

      /// get persist flag val
      base::Result<const aconfig_storage::MutableMappedStorageFile*> GetPersistFlagVal();

      /// get persist flag info
      base::Result<const aconfig_storage::MutableMappedStorageFile*> GetPersistFlagInfo();

      /// check if flag is read only
      base::Result<bool> IsFlagReadOnly(const PackageFlagContext& context);

      /// apply local update to boot flag value copy
      base::Result<void> ApplyLocalOverrideToBootFlagValue();

      private:

      /// container name
      std::string container_;

      // storage record for current container
      StorageRecord storage_record_;

      /// mapped package map file
      std::unique_ptr<aconfig_storage::MappedStorageFile> package_map_;

      /// mapped flag map file
      std::unique_ptr<aconfig_storage::MappedStorageFile> flag_map_;

      /// mapped default flag value file
      std::unique_ptr<aconfig_storage::MappedStorageFile> flag_val_;

      /// mapped boot flag value file
      std::unique_ptr<aconfig_storage::MappedStorageFile> boot_flag_val_;

      /// mapped boot flag info file
      std::unique_ptr<aconfig_storage::MappedStorageFile> boot_flag_info_;

      /// mapped persist flag value file
      std::unique_ptr<aconfig_storage::MutableMappedStorageFile> persist_flag_val_;

      /// mapped persist flag info file
      std::unique_ptr<aconfig_storage::MutableMappedStorageFile> persist_flag_info_;

    }; // class StorageFiles

  } // namespace aconfigd
} // namespace android
