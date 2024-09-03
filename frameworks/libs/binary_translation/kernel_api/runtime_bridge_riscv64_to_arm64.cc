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

#include "berberis/kernel_api/runtime_bridge.h"

#include <cerrno>

#include "berberis/base/macros.h"
#include "berberis/base/tracing.h"

namespace berberis {

long RunGuestSyscall___NR_rt_sigaction(long sig_num_arg,
                                       long act_arg,
                                       long old_act_arg,
                                       long sigset_size_arg) {
  UNUSED(sig_num_arg, act_arg, old_act_arg, sigset_size_arg);
  TRACE("unimplemented syscall __NR_rt_sigaction");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_sigaltstack(long stack, long old_stack) {
  UNUSED(stack, old_stack);
  TRACE("unimplemented syscall __NR_sigaltstack");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_timer_create(long arg_1, long arg_2, long arg_3) {
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_timer_create");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_exit(long arg) {
  UNUSED(arg);
  TRACE("unimplemented syscall __NR_exit");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_clone(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_clone");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_mmap(long arg_1,
                               long arg_2,
                               long arg_3,
                               long arg_4,
                               long arg_5,
                               long arg_6) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
  TRACE("unimplemented syscall __NR_mmap");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_mmap2(long arg_1,
                                long arg_2,
                                long arg_3,
                                long arg_4,
                                long arg_5,
                                long arg_6) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
  TRACE("unimplemented syscall __NR_mmap2");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_munmap(long arg_1, long arg_2) {
  UNUSED(arg_1, arg_2);
  TRACE("unimplemented syscall __NR_munmap");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_mprotect(long arg_1, long arg_2, long arg_3) {
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_mprotect");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_mremap(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_mremap");
  errno = ENOSYS;
  return -1;
}

}  // namespace berberis
