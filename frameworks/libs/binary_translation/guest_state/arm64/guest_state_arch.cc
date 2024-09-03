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

#include "berberis/guest_state/guest_state.h"

#include "berberis/base/checks.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

void SetReturnValueRegister(CPUState& cpu, GuestAddr val) {
  cpu.x[0] = val;
}

GuestAddr GetReturnValueRegister(const CPUState& cpu) {
  return cpu.x[0];
}

void SetStackRegister(CPUState& cpu, GuestAddr val) {
  cpu.sp = val;
}

GuestAddr GetStackRegister(const CPUState& cpu) {
  return cpu.sp;
}

void SetLinkRegister(CPUState& cpu, GuestAddr val) {
  cpu.x[30] = val;
}

GuestAddr GetLinkRegister(const CPUState& cpu) {
  return cpu.x[30];
}

void SetTlsAddr(ThreadState& state, GuestAddr addr) {
  state.tls = addr;
}

GuestAddr GetTlsAddr(const ThreadState& state) {
  return state.tls;
}

void SetShadowCallStackPointer(CPUState& cpu, GuestAddr scs_sp) {
  cpu.x[18] = scs_sp;
}

void AdvanceInsnAddrBeyondSyscall(CPUState& cpu) {
  cpu.insn_addr += 4;
}

std::size_t GetThreadStateRegOffset(int reg) {
  return offsetof(ThreadState, cpu.x[reg]);
}
std::size_t GetThreadStateFRegOffset(int /* reg */) {
  CHECK(false);
}
std::size_t GetThreadStateVRegOffset(int /* reg */) {
  CHECK(false);
}
std::size_t GetThreadStateSimdRegOffset(int reg) {
  return offsetof(ThreadState, cpu.v[reg]);
}

bool IsSimdOffset(size_t offset) {
  size_t v0_offset = offsetof(ThreadState, cpu.v);
  return (offset >= v0_offset) && ((offset - v0_offset) < sizeof(ThreadState::cpu.v));
}

bool DoesCpuStateHaveFlags() {
  return true;
}

bool DoesCpuStateHaveDedicatedFpRegs() {
  return false;
}

bool DoesCpuStateHaveDedicatedVecRegs() {
  return false;
}

bool DoesCpuStateHaveDedicatedSimdRegs() {
  return true;
}

std::size_t GetThreadStateFlagOffset() {
  return offsetof(ThreadState, cpu.flags);
}

}  // namespace berberis
