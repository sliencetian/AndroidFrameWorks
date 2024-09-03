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

  .globl berberis_RawSyscallImpl
  .balign 16

berberis_RawSyscallImpl:
  // The aapcs64 ABI passes the first RawSyscallImpl param (the syscall number) in x0.
  // The arm64 syscall ABI passes the syscall number in x8.

  // Move the syscall number up.
  mov x8, x0

  // Shift the arguments down.
  mov x0, x1
  mov x1, x2
  mov x2, x3
  mov x3, x4
  mov x4, x5
  mov x5, x6

  svc #0

  ret
