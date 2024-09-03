/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include "Apex.h"

#include <android-base/format.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "com_android_apex.h"
#include "constants-private.h"

using android::base::StartsWith;

namespace android::vintf::apex {

static bool isApexReady(PropertyFetcher* propertyFetcher) {
#ifdef LIBVINTF_TARGET
    return propertyFetcher->getBoolProperty("apex.all.ready", false);
#else
    // When running on host, it assumes that /apex is ready.
    // Reason for still relying on PropertyFetcher API is for host-side tests.
    return propertyFetcher->getBoolProperty("apex.all.ready", true);
#endif
}

static status_t GetVintfDirs(FileSystem* fileSystem, PropertyFetcher* propertyFetcher,
                             std::vector<std::string>* dirs, std::string* error,
                             std::function<bool(const std::string&)> filter) {
    std::string apexInfoFile = details::kApexInfoFile;
    std::string apexDir = "/apex";
    if (!isApexReady(propertyFetcher)) {
        apexInfoFile = details::kBootstrapApexInfoFile;
        apexDir = "/bootstrap-apex";
    }

    // Load apex-info-list
    std::string xml;
    auto status = fileSystem->fetch(apexInfoFile, &xml, error);
    if (status == NAME_NOT_FOUND) {
        if (error) {
            error->clear();
        }
        return OK;
    }
    if (status != OK) return status;

    auto apexInfoList = com::android::apex::parseApexInfoList(xml.c_str());
    if (!apexInfoList.has_value()) {
        if (error) {
            *error = std::string("Not a valid XML: ") + apexInfoFile;
        }
        return UNKNOWN_ERROR;
    }

    // Get vendor apex vintf dirs
    for (const auto& apexInfo : apexInfoList->getApexInfo()) {
        // Skip non-active apexes
        if (!apexInfo.getIsActive()) continue;
        // Skip if no preinstalled paths. This shouldn't happen but XML schema says it's optional.
        if (!apexInfo.hasPreinstalledModulePath()) continue;

        const std::string& path = apexInfo.getPreinstalledModulePath();
        if (filter(path)) {
            dirs->push_back(fmt::format("{}/{}/" VINTF_SUB_DIR, apexDir, apexInfo.getModuleName()));
        }
    }
    LOG(INFO) << "Loaded APEX Infos from " << apexInfoFile;
    return OK;
}

std::optional<timespec> GetModifiedTime(FileSystem* fileSystem, PropertyFetcher* propertyFetcher) {
    std::string apexInfoFile = details::kApexInfoFile;
    if (!isApexReady(propertyFetcher)) {
        apexInfoFile = details::kBootstrapApexInfoFile;
    }

    timespec mtime{};
    std::string error;
    status_t status = fileSystem->modifiedTime(apexInfoFile, &mtime, &error);
    if (status == NAME_NOT_FOUND) {
        return std::nullopt;
    }
    if (status != OK) {
        LOG(ERROR) << error;
        return std::nullopt;
    }
    return mtime;
}

status_t GetDeviceVintfDirs(FileSystem* fileSystem, PropertyFetcher* propertyFetcher,
                            std::vector<std::string>* dirs, std::string* error) {
    return GetVintfDirs(fileSystem, propertyFetcher, dirs, error, [](const std::string& path) {
        return StartsWith(path, "/vendor/apex/") || StartsWith(path, "/system/vendor/apex/") ||
               StartsWith(path, "/odm/apex/") || StartsWith(path, "/system/odm/apex/");
    });
}

status_t GetFrameworkVintfDirs(FileSystem* fileSystem, PropertyFetcher* propertyFetcher,
                               std::vector<std::string>* dirs, std::string* error) {
    return GetVintfDirs(fileSystem, propertyFetcher, dirs, error, [](const std::string& path) {
        return StartsWith(path, "/system/apex/") || StartsWith(path, "/system_ext/apex/") ||
               StartsWith(path, "/system/system_ext/apex/") || StartsWith(path, "/product/apex/") ||
               StartsWith(path, "/system/product/apex/");
    });
}

}  // namespace android::vintf::apex
