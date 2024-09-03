/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "gtest/gtest.h"

#include "berberis/guest_loader/guest_loader.h"

#include <android/dlext.h>

#include <string>

#include "berberis/runtime/berberis.h"

namespace berberis {

namespace {

const constexpr uint64_t kNamespaceTypeIsolated = 1;

TEST(guest_loader, smoke) {
  InitBerberis();

  std::string error_msg;
  GuestLoader* loader = GuestLoader::StartAppProcessInNewThread(&error_msg);
  ASSERT_NE(loader, nullptr) << error_msg;

  // Reset dlerror.
  loader->DlError();
  ASSERT_EQ(loader->DlError(), nullptr);

  Dl_info info;
  ASSERT_EQ(loader->DlAddr(loader, &info), 0);
  ASSERT_EQ(loader->DlError(), nullptr);  // dladdr doesn't set dlerror.

  void* handle = loader->DlOpen("libc.so", RTLD_NOW);
  ASSERT_NE(handle, nullptr) << loader->DlError();  // dlerror called only if assertion fails.
  // Clear dlerror: successful dlopen(libc.so) might result in dlerror
  // being set (because of failed dlsym("swift_demangle") during its
  // initialization).
  loader->DlError();

  handle = loader->DlOpen("libdl.so", RTLD_NOW);
  const char* dlerror = loader->DlError();
  ASSERT_NE(handle, nullptr) << dlerror;
  ASSERT_EQ(dlerror, nullptr) << dlerror;

  android_namespace_t* ns = loader->CreateNamespace("classloader-namespace",
                                                    nullptr,
                                                    "/data:/mnt/expand",
                                                    kNamespaceTypeIsolated,
                                                    "/data:/mnt/expand",
                                                    nullptr);
  ASSERT_NE(ns, nullptr) << loader->DlError();  // dlerror called only if assertion fails.
  ASSERT_EQ(loader->DlError(), nullptr);
}

}  // namespace

}  // namespace berberis
