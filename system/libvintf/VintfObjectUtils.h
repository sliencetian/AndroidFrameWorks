/*
 * Copyright (C) 2021 The Android Open Source Project
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

// Special utils for VintfObject(s).

#pragma once

// This is okay because it is a header private to libvintf. Do not do this in exported headers!
#include <android-base/logging.h>

#include <vintf/VintfObject.h>

namespace android {
namespace vintf {
namespace details {

// Get() fetches data and caches it in LockedSharedPtr. The cached data will be
// invalidated when `lastModified` is changed from the last call. Typically `lastModified`
// can refer to the "last modified" timestamp of the data source.
template <typename T, typename F>
std::shared_ptr<const T> Get(const char* id, LockedSharedPtr<T>* ptr, const F& fetch,
                             const std::optional<timespec>& lastModified = std::nullopt) {
    std::unique_lock<std::mutex> lock(ptr->mutex);
    // Check if the last fetched data is fresh. If it's old, re-fetch the data
    // with the new timestamp.
    if (ptr->object && ptr->lastModified != lastModified) {
        LOG(INFO) << id << ": Reloading VINTF information.";
        ptr->object = nullptr;
    }
    if (!ptr->object) {
        LOG(INFO) << id << ": Reading VINTF information.";
        ptr->object = std::make_unique<T>();
        ptr->lastModified = lastModified;
        std::string error;
        status_t status = fetch(ptr->object.get(), &error);
        if (status == OK) {
            LOG(INFO) << id << ": Successfully processed VINTF information";
        } else {
            // Doubled because a malformed error std::string might cause us to
            // lose the status.
            LOG(ERROR) << id << ": status from fetching VINTF information: " << status;
            LOG(ERROR) << id << ": " << status << " VINTF parse error: " << error;
            ptr->object = nullptr;  // frees the old object
        }
    }
    return ptr->object;
}

}  // namespace details
}  // namespace vintf
}  // namespace android
