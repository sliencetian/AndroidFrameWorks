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

#ifndef BERBERIS_GUEST_STATE_GUEST_STATE_ARCH_H_
#define BERBERIS_GUEST_STATE_GUEST_STATE_ARCH_H_

#include <atomic>

#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state_opaque.h"
#include "native_bridge_support/arm/guest_state/guest_state_cpu_state.h"

namespace berberis {

// Guest CPU state + interface to access guest memory.
struct ThreadState {
  CPUState cpu;

  // Guest thread pointer.
  GuestThread* thread;

  // Guest TLS pointer.
  // It can be read using MRC instruction.
  // Statically linked ARM executable initializes it by set_tls syscall.
  // For PIC objects, InitThreadState sets it either to host TLS or to (stub) thread-id.
  // TODO(b/36890513): guest should have its own TLS area for PIC objects too.
  GuestAddr tls;

  // Keep pending signals status here for fast checking in generated code.
  // TODO(b/28058920): move to GuestThread!
  std::atomic_uint_least8_t pending_signals_status;

  GuestThreadResidence residence;

  // Arbitrary per-thread data added by instrumentation.
  void* instrument_data;

  // Point to the guest thread memory start position.
  void* thread_state_storage;
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_STATE_GUEST_STATE_ARCH_H_
