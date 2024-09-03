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

#include <jni.h>
#include <signal.h>
#include <stdlib.h>

#include "android/log.h"

#define COVERAGE_FLUSH_SIGNAL (__SIGRTMIN + 5)
const char *TAG = "CoverageDumper";

// Retrieve the method registered for SIG COVERAGE_FLUSH_SIGNAL(37) and
// execute it. This method can only be executed once in the lifetime of a
// process and MUST BE executed before the process exits. Successive executions
// will result in noops (No operations).
//
// More context:
// https://cs.android.com/android/platform/superproject/main/+/main:system/extras/toolchain-extras/profile-clang-extras.cpp;l=52
// Everytime a piece of code (eg: shared_library gets loaded, a process starts),
// `init_profile_extras` gets executed, that method chains coverge dump methods.
//
// What we do here is that we pick the head of the chain and execute it.
JNIEXPORT void
Java_com_android_modules_utils_testing_NativeCoverageHackInstrumentationListener_dumpCoverage(
    JNIEnv *env) {
  sighandler_t ret = signal(COVERAGE_FLUSH_SIGNAL, SIG_IGN);
  if (ret != SIG_ERR && ret != SIG_IGN && ret != SIG_DFL) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "Coverage dumped.");
    // The signum is unused.
    (ret)(/* signum */ COVERAGE_FLUSH_SIGNAL);
  } else {
    // Clang did not register its signal handler which means that the code
    // was not compiled under coverage variant.
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "No coverage signal registered! No-op");
  }
}