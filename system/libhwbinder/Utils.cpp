/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "Utils.h"
#include <hwbinder/HidlSupport.h>

#include <string.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

namespace android::hardware {

void zeroMemory(uint8_t* data, size_t size) {
    memset(data, 0, size);
}

static bool file_exists(const std::string& file) {
    int res = access(file.c_str(), F_OK);
    if (res == 0 || errno == EACCES) return true;
    return false;
}

static bool isHwServiceManagerInstalled() {
    return file_exists("/system_ext/bin/hwservicemanager") ||
           file_exists("/system/system_ext/bin/hwservicemanager") ||
           file_exists("/system/bin/hwservicemanager");
}

static bool waitForHwServiceManager() {
    if (!isHwServiceManagerInstalled()) {
        return false;
    }
    // TODO(b/31559095): need bionic host so that we can use 'prop_info' returned
    // from WaitForProperty
#ifdef __ANDROID__
    static const char* kHwServicemanagerReadyProperty = "hwservicemanager.ready";

    using std::literals::chrono_literals::operator""s;

    using android::base::WaitForProperty;
    while (true) {
        if (base::GetBoolProperty("hwservicemanager.disabled", false)) {
            return false;
        }
        if (WaitForProperty(kHwServicemanagerReadyProperty, "true", 1s)) {
            return true;
        }
        LOG(WARNING) << "Waited for hwservicemanager.ready for a second, waiting another...";
    }
#endif  // __ANDROID__
    return true;
}

bool isHwbinderSupportedBlocking() {
    return waitForHwServiceManager();
}
}   // namespace android::hardware
