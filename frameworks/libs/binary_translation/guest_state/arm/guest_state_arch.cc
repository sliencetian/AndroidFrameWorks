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

#include "berberis/guest_state/guest_state.h"

#include "berberis/base/macros.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

void SetReturnValueRegister(CPUState& cpu, GuestAddr val) {
  cpu.r[0] = val;
}

GuestAddr GetReturnValueRegister(const CPUState& cpu) {
  return cpu.r[0];
}

void SetStackRegister(CPUState& cpu, GuestAddr val) {
  cpu.r[13] = val;
}

GuestAddr GetStackRegister(const CPUState& cpu) {
  return cpu.r[13];
}

void SetLinkRegister(CPUState& cpu, GuestAddr val) {
  cpu.r[14] = val;
}

GuestAddr GetLinkRegister(const CPUState& cpu) {
  return cpu.r[14];
}

void SetTlsAddr(ThreadState& state, GuestAddr addr) {
  state.tls = addr;
}

GuestAddr GetTlsAddr(const ThreadState& state) {
  return state.tls;
}

void SetShadowCallStackPointer(CPUState& cpu, GuestAddr scs_sp) {
  UNUSED(cpu, scs_sp);
}

void AdvanceInsnAddrBeyondSyscall(CPUState& cpu) {
  if (cpu.insn_addr % 2 == 0) {
    cpu.insn_addr += 4;
  } else {
    // Thumb SVC is always 2 bytes.
    cpu.insn_addr += 2;
  }
}

}  // namespace berberis
