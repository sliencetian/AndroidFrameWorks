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

#define LOG_TAG "nativebridgesupport"

#include <android-base/properties.h>
#include <dlfcn.h>
#include <log/log_main.h>

#include "native_bridge_support/guest_state_accessor/accessor.h"

#if defined(__ANDROID__)
#include "native_bridge_support/guest_state_accessor/dlext_namespaces.h"
#endif

void* OpenSystemLibrary(const char* path, int flags) {
#if defined(__ANDROID__)
  // The system namespace is called "default" for binaries in /system and
  // "system" for those in the Runtime APEX. Try "system" first since
  // "default" always exists.
  // TODO(b/185587109): Get rid of this error prone logic.
  android_namespace_t* system_ns = android_get_exported_namespace("system");
  if (system_ns == nullptr) {
    system_ns = android_get_exported_namespace("default");
    if (system_ns == nullptr) {
      ALOGE("Failed to get system namespace for loading %s", path);
    }
  }
  const android_dlextinfo dlextinfo = {
      .flags = ANDROID_DLEXT_USE_NAMESPACE,
      .library_namespace = system_ns,
  };

  return android_dlopen_ext(path, flags, &dlextinfo);
#else
  return dlopen(path, flags);
#endif
}

int LoadGuestStateRegisters(const void* guest_state_data,
                            size_t guest_state_data_size,
                            NativeBridgeGuestRegs* guest_regs) {
  std::string library_name = android::base::GetProperty(
      "ro.dalvik.vm.native.bridge", /*default_value=*/"");
  if (library_name.empty()) {
    return NATIVE_BRIDGE_GUEST_STATE_ACCESSOR_ERROR_INVALID_STATE;
  }

  void *proxy = OpenSystemLibrary(library_name.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!proxy) {
    ALOGE("dlopen failed: %s: %s", library_name.c_str(), dlerror());
    return NATIVE_BRIDGE_GUEST_STATE_ACCESSOR_ERROR_INVALID_STATE;
  }

  using LoadGuestStateRegistersFunc =
      int (*)(const void *, size_t, NativeBridgeGuestRegs *);
  LoadGuestStateRegistersFunc LoadGuestStateRegistersImpl =
      reinterpret_cast<LoadGuestStateRegistersFunc>(
          dlsym(proxy, "LoadGuestStateRegisters"));
  if (!LoadGuestStateRegistersImpl) {
    ALOGE("failed to initialize proxy library LoadGuestStateRegisters: %s", dlerror());
    return NATIVE_BRIDGE_GUEST_STATE_ACCESSOR_ERROR_INVALID_STATE;
  }

  return LoadGuestStateRegistersImpl(guest_state_data, guest_state_data_size, guest_regs);
}
