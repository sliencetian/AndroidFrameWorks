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
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

// WARNING: this code is part of libhwbinder, a fork of libbinder. Generally,
// this means that it is only relevant to HIDL. Any AIDL- or libbinder-specific
// code should not try to use these things.
namespace android::hardware {
// Return whether or not hwbinder is supported on this device based on the existence
// of hwservicemanager.
//
// If the service is installed on the device, this method blocks and waits for
// hwservicemanager to be either ready or disabled.
//
// This function will block during early init while hwservicemanager is
// starting. If hwbinder is supported on the device, it waill wait until
// the hwservicemanager.ready property is set to true. If hwbinder is not supported
// but hwservicemanager is still installed on the device, it will wait
// until hwservicemanager.enabled is set to false.
//
// return - false if the service isn't installed on the device
//          false if the service is installed, but disabled
//          true if the service is ready
bool isHwbinderSupportedBlocking();
} // namespace android::hardware

