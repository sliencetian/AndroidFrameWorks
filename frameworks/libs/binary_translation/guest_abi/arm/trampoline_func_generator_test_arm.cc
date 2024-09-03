/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "berberis/base/bit_util.h"
#include "berberis/guest_abi/function_wrappers.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/host_function_wrapper_impl.h"

namespace berberis {

namespace {

TEST(TrampolineFuncGenerator, IntRes) {
  struct Callee {
    static int foo() { return 1; }
  };

  TrampolineFunc func = GetTrampolineFunc<int(void)>();

  ProcessState state{};

  func(reinterpret_cast<void*>(Callee::foo), &state);

  EXPECT_EQ(1u, state.cpu.r[0]);
}

TEST(TrampolineFuncGenerator, FloatArgs) {
  struct Callee {
    static void foo(void* p, float x, float y) {
      EXPECT_EQ(nullptr, p);
      EXPECT_EQ(0.5f, x);
      EXPECT_EQ(0.75f, y);
    }
  };

  TrampolineFunc func = GetTrampolineFunc<void(void*, float, float)>();

  ProcessState state{};

  state.cpu.r[0] = 0u;
  state.cpu.r[1] = bit_cast<uint32_t>(0.5f);
  state.cpu.r[2] = bit_cast<uint32_t>(0.75f);

  EXPECT_NO_FATAL_FAILURE(func(reinterpret_cast<void*>(Callee::foo), &state));
}

}  // namespace

}  // namespace berberis
