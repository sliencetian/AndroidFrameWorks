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
#include <aconfigd.pb.h>

#include "storage_files_manager.h"

namespace android {
  namespace aconfigd {

    /// Aconfigd socket name
    static constexpr char kAconfigdSocket[] = "aconfigd";

    /// Aconfigd root dir
    static constexpr char kAconfigdRootDir[] = "/metadata/aconfig";

    /// Persistent storage records pb file full path
    static constexpr char kPersistentStorageRecordsFileName[] =
        "/metadata/aconfig/storage_records.pb";

  class Aconfigd {
    public:

    /// constructor
    Aconfigd(const std::string& root_dir,
             const std::string& persist_storage_records)
        : root_dir_(root_dir)
        , persist_storage_records_(persist_storage_records)
        , storage_files_manager_(nullptr) {
      storage_files_manager_.reset(new StorageFilesManager(root_dir_));
    }

    /// destructor
    ~Aconfigd() = default;

    /// no copy
    Aconfigd(const Aconfigd&) = delete;
    Aconfigd& operator=(const Aconfigd&) = delete;

    /// move constructor and assignment
    Aconfigd(Aconfigd&& rhs)
        : root_dir_(rhs.root_dir_)
        , persist_storage_records_(rhs.persist_storage_records_)
        , storage_files_manager_(std::move(rhs.storage_files_manager_))
    {}
    Aconfigd& operator=(Aconfigd&& rhs) = delete;

    public:

    /// Initialize in memory aconfig storage records
    base::Result<void> InitializeInMemoryStorageRecords();

    /// Initialize platform RO partition flag storage
    base::Result<void> InitializePlatformStorage();

    /// Initialize mainline flag storage
    base::Result<void> InitializeMainlineStorage();

    /// Handle incoming messages to aconfigd socket
    base::Result<void> HandleSocketRequest(const StorageRequestMessage& message,
                                     StorageReturnMessage& return_message);

    private:

    /// Handle a flag override request
    base::Result<void> HandleFlagOverride(
        const StorageRequestMessage::FlagOverrideMessage& msg,
        StorageReturnMessage& return_msg);

    /// Handle OTA flag staging request
    base::Result<void> HandleOTAStaging(
        const StorageRequestMessage::OTAFlagStagingMessage& msg,
        StorageReturnMessage& return_msg);

    /// Handle new storage request
    base::Result<void> HandleNewStorage(
        const StorageRequestMessage::NewStorageMessage& msg,
        StorageReturnMessage& return_msg);

    /// Handle a flag query request
    base::Result<void> HandleFlagQuery(
        const StorageRequestMessage::FlagQueryMessage& msg,
        StorageReturnMessage& return_msg);

    /// Handle override removal request
    base::Result<void> HandleLocalOverrideRemoval(
        const StorageRequestMessage::RemoveLocalOverrideMessage& msg,
        StorageReturnMessage& return_msg);

    /// Handle storage reset
    base::Result<void> HandleStorageReset(StorageReturnMessage& return_msg);

    /// Handle list storage
    base::Result<void> HandleListStorage(
        const StorageRequestMessage::ListStorageMessage& msg,
        StorageReturnMessage& return_message);

    /// Read OTA flag overrides to be applied for current build
    base::Result<std::vector<FlagOverride>> ReadOTAFlagOverridesToApply();

    /// Write remaining OTA flag overrides back to pb file
    base::Result<void> WriteRemainingOTAOverrides(
        const std::vector<FlagOverride>& ota_flags);

    private:

    /// root storage dir
    const std::string root_dir_;

    /// persist storage records pb file
    const std::string persist_storage_records_;

    /// storage files manager
    std::unique_ptr<StorageFilesManager> storage_files_manager_;
  };

  } // namespace aconfigd
} // namespace android
