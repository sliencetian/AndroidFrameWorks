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

#include "gtest/gtest.h"

#include <cstddef>
#include <cstring>

#include "berberis/guest_state/get_cpu_state_opaque.h"
#include "berberis/guest_state/guest_state_arch.h"
#include "native_bridge_support/guest_state_accessor/accessor.h"

namespace berberis {

namespace {

TEST(GetCpuStateTest, TestValuesSet) {
  NativeBridgeGuestRegs guest_regs{.guest_arch = NATIVE_BRIDGE_ARCH_RISCV64};
  CPUState cpu_state;
  for (size_t off = 0; off < sizeof(CPUState); off++) {
    auto val = off % 199;  // 199 is prime to avoid regularly repeating values in registers
    memcpy(reinterpret_cast<char*>(&cpu_state) + off, &val, 1);
  }

  EXPECT_EQ(GetCpuState(&guest_regs, &cpu_state), 0);

  for (std::size_t i = 0; i < 32; i++) {
    EXPECT_EQ(guest_regs.regs_riscv64.x[i], cpu_state.x[i]);
  }
  for (std::size_t i = 0; i < 32; i++) {
    EXPECT_EQ(guest_regs.regs_riscv64.f[i], cpu_state.f[i]);
  }
  for (std::size_t i = 0; i < 32; i++) {
    EXPECT_EQ(guest_regs.regs_riscv64.v[i], cpu_state.v[i]);
  }
  EXPECT_EQ(guest_regs.regs_riscv64.ip, cpu_state.insn_addr);
}

TEST(GetArm64CpuStateTest, TestErrorSize) {
  NativeBridgeGuestRegs guest_regs{.guest_arch = NATIVE_BRIDGE_ARCH_RISCV64};
  int res = LoadGuestStateRegisters(nullptr, sizeof(ThreadState) - 1, &guest_regs);
  EXPECT_EQ(res, NATIVE_BRIDGE_GUEST_STATE_ACCESSOR_ERROR_INVALID_STATE);
}

TEST(GetArm64CpuStateTest, TestErrorArch) {
  NativeBridgeGuestRegs guest_regs{.guest_arch = NATIVE_BRIDGE_ARCH_ARM64};
  CPUState cpu_state;
  int res = GetCpuState(&guest_regs, &cpu_state);
  EXPECT_EQ(res, NATIVE_BRIDGE_GUEST_STATE_ACCESSOR_ERROR_UNSUPPORTED_ARCH);
}

}  // namespace

}  // namespace berberis