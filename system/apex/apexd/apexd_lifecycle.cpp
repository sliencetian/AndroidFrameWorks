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

#include <chrono>
#include <thread>

#include "apexd.h"
#include "apexd_lifecycle.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

#include "apexd_utils.h"

using android::base::GetProperty;
using android::base::Result;
using android::base::WaitForProperty;

constexpr int MAX_WAIT_COUNT = 60;
constexpr int WAIT_DURATION_SECONDS = 10;
static const char* BOOT_TIMEOUT = "BootTimeout"; // NOLINT

namespace android {
namespace apex {

bool ApexdLifecycle::IsBooting() {
  auto status = GetProperty(kApexStatusSysprop, "");
  return status != kApexStatusReady && status != kApexStatusActivated;
}

void ApexdLifecycle::RevertActiveSessions(const std::string& process,
                                          const std::string& error) {
  auto result = RevertActiveSessionsAndReboot(process, error);
  if (!result.ok()) {
    if (error != BOOT_TIMEOUT) {
      LOG(ERROR) << "Revert failed : " << result.error();
      // Can not anything more but loop until boot successfully
      while (!boot_completed_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      return;
    }
  }
  // This should never be reached
  LOG(FATAL) << "Active sessions were reverted, but reboot wasn't triggered.";
}

void ApexdLifecycle::WaitForBootStatus(const bool has_active_session) {
  int wait_count = 0;
  while (!boot_completed_) {
    // Check for change in either crashing property or sys.boot_completed
    // Wait for updatable_crashing property change for most of the time
    // (arbitrary 10s), briefly check if boot has completed successfully,
    // if not continue waiting for updatable_crashing.
    // We use this strategy so that we can quickly detect if an updatable
    // process is crashing.
    if (WaitForProperty("sys.init.updatable_crashing", "1",
                        std::chrono::seconds(WAIT_DURATION_SECONDS))) {
      auto name = GetProperty("sys.init.updatable_crashing_process_name", "");
      LOG(ERROR) << "Native process '" << (name.empty() ? "[unknown]" : name)
                 << "' is crashing. Attempting a revert";
      RevertActiveSessions(name, "");
    }
    // Check if system stuck in boot screen and revert the staging apex once
    if (has_active_session && ++wait_count == MAX_WAIT_COUNT) {
      LOG(ERROR) << "System didn't finish boot in "
                 << (WAIT_DURATION_SECONDS * MAX_WAIT_COUNT)
                 << " seconds. Attempting a revert";
      RevertActiveSessions("", BOOT_TIMEOUT);
    }
  }
}

void ApexdLifecycle::MarkBootCompleted() { boot_completed_ = true; }

}  // namespace apex
}  // namespace android
