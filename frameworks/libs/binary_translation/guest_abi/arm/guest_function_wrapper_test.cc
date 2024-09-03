/*
 * Copyright (C) 2019 The Android Open Source Project
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
#include "berberis/guest_abi/guest_function_wrapper.h"
#include "berberis/guest_abi/guest_type.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/test_utils/guest_exec_region.h"
#include "berberis/test_utils/translation_test.h"

namespace berberis {

namespace {

class GuestFunctionWrapperTest : public TranslationTest {};

TEST_F(GuestFunctionWrapperTest, WrapNull) {
  using FooPtr = int (*)(int, int);
  EXPECT_EQ(nullptr, berberis::WrapGuestFunction(bit_cast<GuestType<FooPtr>>(0L), "foo"));
  using BarPtr = void (*)(void*);
  EXPECT_EQ(nullptr, berberis::WrapGuestFunction(bit_cast<GuestType<BarPtr>>(0L), "bar"));
}

TEST_F(GuestFunctionWrapperTest, Wrap2Sub) {
  // int sub(int x, int y) {
  //   return x - y;
  // }
  GuestAddr pc = MakeGuestExecRegion<uint32_t>({
      0xe0400001,  // sub r0, r0, r1
      0xe12fff1e,  // bx lr
  });

  using TwoArgFunction = int (*)(int, int);
  TwoArgFunction sub = WrapGuestFunction(bit_cast<GuestType<TwoArgFunction>>(pc), "sub");

  int x = sub(239, 11);
  EXPECT_EQ(228, x);
}

TEST_F(GuestFunctionWrapperTest, Wrap2SubLong) {
  // int64_t sub_long(int64_t x, int64_t y) {
  //   return x - y;
  // }
  GuestAddr pc = MakeGuestExecRegion<uint32_t>({
      0xe0500002,  // subs r0, r0, r2
      0xe0c11003,  // sbc r1, r1, r3
      0xe12fff1e,  // bx lr
  });

  using TwoArgFunction = int64_t (*)(int64_t, int64_t);
  TwoArgFunction sub = WrapGuestFunction(bit_cast<GuestType<TwoArgFunction>>(pc), "sub_long");

  uint64_t x = sub(0xffff0000ffff0001ULL, 0x7fff0000ffff0000ULL);
  EXPECT_EQ(0x8000000000000001ULL, x);
}

TEST_F(GuestFunctionWrapperTest, Wrap2SubFloat) {
  // float sub_float(float x, float y) {
  //   return x - y;
  // }
  GuestAddr pc = MakeGuestExecRegion<uint32_t>({
      0xee070a90,  // vmov s15, r0
      0xee071a10,  // vmov s14, r1
      0xee777ac7,  // vsub.f32 s15, s15, s14
      0xee170a90,  // vmov r0, s15
      0xe12fff1e,  // bx lr
  });

  using TwoArgFunction = float (*)(float, float);
  TwoArgFunction sub = WrapGuestFunction(bit_cast<GuestType<TwoArgFunction>>(pc), "sub_float");

  float x = sub(2.71f, 3.14f);
  EXPECT_FLOAT_EQ(-0.43f, x);
}

TEST_F(GuestFunctionWrapperTest, Wrap2SubDouble) {
  // double sub_double(double x, double y) {
  //   return x - y;
  // }
  GuestAddr pc = MakeGuestExecRegion<uint32_t>({
      0xec410b30,  // vmov d16, r0, r1
      0xec432b31,  // vmov d17, r2, r3
      0xee700be1,  // vsub.f64 d16, d16, d17
      0xec510b30,  // vmov r0, r1, d16
      0xe12fff1e,  // bx lr
  });

  using TwoArgFunction = double (*)(double, double);
  TwoArgFunction sub = WrapGuestFunction(bit_cast<GuestType<TwoArgFunction>>(pc), "sub_double");

  double x = sub(2.71, 3.14);
  EXPECT_DOUBLE_EQ(-0.43, x);
}

}  // namespace

}  // namespace berberis
