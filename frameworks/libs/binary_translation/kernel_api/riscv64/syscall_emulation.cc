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

#include <fcntl.h>  // AT_FDCWD, AT_SYMLINK_NOFOLLOW
#include <linux/sched.h>
#include <linux/unistd.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>

#include <cerrno>

#include "berberis/base/macros.h"
#include "berberis/base/scoped_errno.h"
#include "berberis/base/tracing.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/instrument/syscall.h"
#include "berberis/kernel_api/main_executable_real_path_emulation.h"
#include "berberis/kernel_api/runtime_bridge.h"
#include "berberis/kernel_api/syscall_emulation_common.h"

// TODO(b/346604197): Enable on arm64 once these modules are ported.
#ifdef __x86_64__
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/runtime_primitives/runtime_library.h"
#endif

#include "epoll_emulation.h"
#include "guest_types.h"

namespace berberis {

namespace {

int FstatatForGuest(int dirfd, const char* path, struct stat* buf, int flags) {
  const char* real_path = nullptr;
  if ((flags & AT_SYMLINK_NOFOLLOW) == 0) {
    real_path = TryReadLinkToMainExecutableRealPath(path);
  }
  return syscall(__NR_newfstatat, dirfd, real_path ? real_path : path, buf, flags);
}

void Hwprobe(Guest_riscv_hwprobe& pair) {
  switch (pair.key) {
    case RISCV_HWPROBE_KEY_MVENDORID:
      pair.value = 0;
      break;
    case RISCV_HWPROBE_KEY_MARCHID:
      pair.value = 0;
      break;
    case RISCV_HWPROBE_KEY_MIMPID:
      pair.value = 0;
      break;
    case RISCV_HWPROBE_KEY_BASE_BEHAVIOR:
      pair.value = RISCV_HWPROBE_BASE_BEHAVIOR_IMA;
      break;
    case RISCV_HWPROBE_KEY_IMA_EXT_0:
      pair.value = RISCV_HWPROBE_IMA_FD | RISCV_HWPROBE_IMA_C | RISCV_HWPROBE_IMA_V |
                   RISCV_HWPROBE_EXT_ZBA | RISCV_HWPROBE_EXT_ZBB | RISCV_HWPROBE_EXT_ZBS;
      break;
    case RISCV_HWPROBE_KEY_CPUPERF_0:
      pair.value = RISCV_HWPROBE_MISALIGNED_FAST;
      break;
    default:
      TRACE("unsupported __riscv_hwprobe capability key: %ld", pair.key);
      pair.key = -1;
      pair.value = 0;
      break;
  }
}

long RunGuestSyscall___NR_execveat(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_execveat");
  errno = ENOSYS;
  return -1;
}

// sys_fadvise64 has a different entry-point symbol name between riscv64 and x86_64.
#ifdef __x86_64__
long RunGuestSyscall___NR_fadvise64(long arg_1, long arg_2, long arg_3, long arg_4) {
  // on 64-bit architectures, sys_fadvise64 and sys_fadvise64_64 are equal.
  return syscall(__NR_fadvise64, arg_1, arg_2, arg_3, arg_4);
}
#endif

long RunGuestSyscall___NR_ioctl(long arg_1, long arg_2, long arg_3) {
  // TODO(b/128614662): translate!
  TRACE("unimplemented ioctl 0x%lx, running host syscall as is", arg_2);
  return syscall(__NR_ioctl, arg_1, arg_2, arg_3);
}

long RunGuestSyscall___NR_newfstatat(long arg_1, long arg_2, long arg_3, long arg_4) {
  struct stat host_stat;
  int result = FstatatForGuest(static_cast<int>(arg_1),       // dirfd
                               bit_cast<const char*>(arg_2),  // path
                               &host_stat,
                               static_cast<int>(arg_4));  // flags
  if (result != -1) {
    ConvertHostStatToGuestArch(host_stat, bit_cast<GuestAddr>(arg_3));
  }
  return result;
}

long RunGuestSyscall___NR_riscv_hwprobe(long arg_1,
                                        long arg_2,
                                        long arg_3,
                                        long arg_4,
                                        long arg_5) {
  UNUSED(arg_3, arg_4);  // cpu_count, cpus_in

  // There are currently no flags defined by the kernel. This may change in the future.
  static constexpr unsigned int kFlagsAll = 0;

  auto pairs = bit_cast<Guest_riscv_hwprobe*>(arg_1);
  auto pair_count = bit_cast<size_t>(arg_2);
  auto flags = static_cast<unsigned int>(bit_cast<unsigned long>(arg_5));
  if ((flags & ~kFlagsAll) != 0) {
    return -EINVAL;
  }

  for (size_t i = 0; i < pair_count; ++i) {
    Hwprobe(pairs[i]);
  }
  return 0;
}

long RunGuestSyscall___NR_riscv_flush_icache(long arg_1, long arg_2, long arg_3) {
// TODO(b/346604197): Enable on arm64 once runtime_primitives are ready.
#ifdef __x86_64__
  static constexpr uint64_t kFlagsLocal = 1UL;
  static constexpr uint64_t kFlagsAll = kFlagsLocal;

  // ATTENTION: On RISC-V, arg_2 is the address range end, not the address range size.
  auto start = bit_cast<GuestAddr>(arg_1);
  auto end = bit_cast<GuestAddr>(arg_2);
  auto flags = bit_cast<uint64_t>(arg_3);
  if (end < start || (flags & ~kFlagsAll) != 0) {
    errno = EINVAL;
    return -1;
  }

  // Ignore kFlagsLocal because we do not have a per-thread cache to clear.
  TRACE("icache flush: [0x%lx, 0x%lx)", start, end);
  InvalidateGuestRange(start, end);
  return 0;
#else
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_riscv_flush_icache");
  errno = ENOSYS;
  return -1;
#endif
}

// RunGuestSyscallImpl.
#if defined(__aarch64__)
#include "gen_syscall_emulation_riscv64_to_arm64-inl.h"
#elif defined(__x86_64__)
#include "gen_syscall_emulation_riscv64_to_x86_64-inl.h"
#else
#error "Unsupported host arch"
#endif

}  // namespace

void RunGuestSyscall(ThreadState* state) {
#ifdef __x86_64__
  // ATTENTION: run guest signal handlers instantly!
  // If signal arrives while in a syscall, syscall should immediately return with EINTR.
  // In this case pending signals are OK, as guest handlers will run on return from syscall.
  // BUT, if signal action has SA_RESTART, certain syscalls will restart instead of returning.
  // In this case, pending signals will never run...
  ScopedPendingSignalsDisabler scoped_pending_signals_disabler(state->thread);
#else
  // TODO(b/346604197): Enable on arm64 once guest_os_primitives is ported.
  TRACE("ScopedPendingSignalsDisabler is not available on this arch");
#endif
  ScopedErrno scoped_errno;

  long guest_nr = state->cpu.x[A7];
  if (kInstrumentSyscalls) {
    OnSyscall(state, guest_nr);
  }

  // RISCV Linux takes arguments in a0-a5 and syscall number in a7.
  // TODO(b/161722184): if syscall is interrupted by signal, signal handler might overwrite the
  // return value, so setting A0 here might be incorrect. Investigate!
  long result = RunGuestSyscallImpl(guest_nr,
                                    state->cpu.x[A0],
                                    state->cpu.x[A1],
                                    state->cpu.x[A2],
                                    state->cpu.x[A3],
                                    state->cpu.x[A4],
                                    state->cpu.x[A5]);
  if (result == -1) {
    state->cpu.x[A0] = -errno;
  } else {
    state->cpu.x[A0] = result;
  }

  if (kInstrumentSyscalls) {
    OnSyscallReturn(state, guest_nr);
  }
}

}  // namespace berberis
