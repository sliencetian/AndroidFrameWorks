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

#include <linux/unistd.h>
#include <sched.h>
#include <semaphore.h>

#include <cstring>  // strerror

#include "berberis/base/checks.h"
#include "berberis/base/tracing.h"
#include "berberis/guest_os_primitives/guest_signal.h"
#include "berberis/guest_os_primitives/guest_thread.h"
#include "berberis/guest_os_primitives/guest_thread_manager.h"  // ResetCurrentGuestThreadAfterFork
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state_opaque.h"
#include "berberis/runtime/execute_guest.h"
#include "berberis/runtime_primitives/runtime_library.h"

#include "guest_signal_action.h"
#include "guest_thread_manager_impl.h"
#include "scoped_signal_blocker.h"

namespace berberis {

namespace {

long CloneSyscall(long flags, long child_stack, long parent_tid, long new_tls, long child_tid) {
#if defined(__x86_64__)  // sys_clone's last two arguments are flipped on x86-64.
  return syscall(__NR_clone, flags, child_stack, parent_tid, child_tid, new_tls);
#else
  return syscall(__NR_clone, flags, child_stack, parent_tid, new_tls, child_tid);
#endif
}

struct GuestThreadCloneInfo {
  GuestThread* thread;
  HostSigset mask;
  sem_t sem;
};

void SemPostOrDie(sem_t* sem) {
  int error = sem_post(sem);
  // sem_post works in two stages: it increments semaphore's value, and then calls FUTEX_WAKE.
  // If FUTEX_WAIT sporadically returns inside sem_wait between sem_post stages then sem_wait
  // may observe the updated value and successfully finish. If semaphore is destroyed upon
  // sem_wait return (like in CloneGuestThread), sem_post's call to FUTEX_WAKE will fail with
  // EINVAL.
  // Note that sem_destroy itself may do nothing (bionic and glibc are like that), the actual
  // destruction happens because we free up memory (e.g. stack frame) where sem_t is stored.
  // More details at https://sourceware.org/bugzilla/show_bug.cgi?id=12674
#if defined(__GLIBC__) && ((__GLIBC__ < 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ < 21)))
  // GLibc before 2.21 may return EINVAL in the above situation. We ignore it since we cannot do
  // anything about it, and it doesn't really break anything: we just acknowledge the fact that the
  // semaphore can be destoyed already.
  LOG_ALWAYS_FATAL_IF(error != 0 && error != EINVAL, "sem_post returned error=%s", strerror(errno));
#else
  // Bionic and recent GLibc ignore the error code returned
  // from FUTEX_WAKE. So, they never return EINVAL.
  LOG_ALWAYS_FATAL_IF(error != 0, "sem_post returned error=%s", strerror(errno));
#endif
}

int RunClonedGuestThread(void* arg) {
  GuestThreadCloneInfo* info = static_cast<GuestThreadCloneInfo*>(arg);
  GuestThread* thread = info->thread;

  // Cannot use host pthread_key!
  // TODO(b/280551726): Clear guest thread in exit syscall.
  InsertCurrentThread(thread, false);

  // ExecuteGuest requires pending signals enabled.
  ScopedPendingSignalsEnabler scoped_pending_signals_enabler(thread);

  // Host signals are blocked in parent before the clone,
  // and remain blocked in child until this point.
  RTSigprocmaskSyscallOrDie(SIG_SETMASK, &info->mask, nullptr);

  // Notify parent that child is ready. Now parent can:
  // - search for child in thread table
  // - send child a signal
  // - dispose info
  SemPostOrDie(&info->sem);
  // TODO(b/77574158): Ensure caller has a chance to handle the notification.
  sched_yield();

  ExecuteGuest(thread->state());

  LOG_ALWAYS_FATAL("cloned thread didn't exit");
  return 0;
}

}  // namespace

// go/berberis-guest-threads
pid_t CloneGuestThread(GuestThread* thread,
                       int flags,
                       GuestAddr guest_stack_top,
                       GuestAddr parent_tid,
                       GuestAddr new_tls,
                       GuestAddr child_tid) {
  ThreadState& thread_state = *thread->state();
  if (!(flags & CLONE_VM)) {
    // Memory is *not* shared with the child.
    // Run the child on the same host stack as the parent. Thus, can use host local variables.
    // The child gets a copy of guest thread object.
    // ATTENTION: Do not set new tls for the host - tls might be incompatible.
    // TODO(b/280551726): Consider forcing new host tls to 0.
    long pid = CloneSyscall(flags & ~CLONE_SETTLS, 0, parent_tid, 0, child_tid);
    if (pid == 0) {
      // Child, reset thread table.
      ResetCurrentGuestThreadAfterFork(thread);
      if (guest_stack_top) {
        SetStackRegister(GetCPUState(thread_state), guest_stack_top);
        // TODO(b/280551726): Reset stack attributes?
      }
      if ((flags & CLONE_SETTLS)) {
        SetTlsAddr(thread_state, new_tls);
      }
    }
    return pid;
  }

  // Memory is shared with the child.
  // The child needs a distinct stack, both host and guest! Because of the distinct host stack,
  // cannot use host local variables. For now, use clone function to pass parameters to the child.
  // The child needs new instance of guest thread object.

  GuestThreadCloneInfo info;

  info.thread = GuestThread::CreateClone(thread, (flags & CLONE_SIGHAND) != 0);
  if (info.thread == nullptr) {
    return EAGAIN;
  }

  ThreadState& clone_thread_state = *info.thread->state();

  if ((flags & CLONE_SETTLS)) {
    SetTlsAddr(clone_thread_state, new_tls);
  }

  // Current insn addr is on SVC instruction, move to the next.
  // TODO(b/280551726): Not needed if we can use raw syscall and continue current execution.
  CPUState& clone_cpu = GetCPUState(clone_thread_state);
  AdvanceInsnAddrBeyondSyscall(clone_cpu);
  SetReturnValueRegister(clone_cpu, 0);  // Syscall return value

  if (guest_stack_top != kNullGuestAddr) {
    SetStackRegister(GetCPUState(clone_thread_state), guest_stack_top);
    SetLinkRegister(clone_cpu, kNullGuestAddr);
  } else {
    if (!(flags & CLONE_VFORK)) {
      TRACE("CLONE_VM with NULL guest stack and not in CLONE_VFORK mode, returning EINVAL");
      return EINVAL;
    }
    // See b/323981318 and b/156400255.
    TRACE("CLONE_VFORK with CLONE_VM and NULL guest stack, will share guest stack with parent");
    // GuestThread::CreateClone has already copied stack and link pointers to new thread.
  }

  // Thread must start with pending signals while it's executing runtime code.
  SetPendingSignalsStatusAtomic(clone_thread_state, kPendingSignalsEnabled);
  SetResidence(clone_thread_state, kOutsideGeneratedCode);

  int error = sem_init(&info.sem, 0, 0);
  LOG_ALWAYS_FATAL_IF(error != 0, "sem_init returned error=%s", strerror(errno));

  // ATTENTION: Don't set new tls for the host - tls might be incompatible.
  // TODO(b/280551726): Consider forcing new host tls to 0.
  long pid;
  {
    ScopedSignalBlocker signal_blocker;
    info.mask = *signal_blocker.old_mask();
    pid = clone(RunClonedGuestThread,
                info.thread->GetHostStackTop(),
                flags & ~CLONE_SETTLS,
                &info,
                parent_tid,
                nullptr,
                child_tid);
    if (pid != -1) {
      CHECK_EQ(0, sem_wait(&info.sem));  // Wait with blocked signals to avoid EINTR.
    }
  }

  if (pid == -1) {
    GuestThread::Destroy(info.thread);
  }

  sem_destroy(&info.sem);
  return pid;
}

}  // namespace berberis
