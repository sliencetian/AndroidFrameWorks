#!/usr/bin/python
#
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import json
import sys


def _print_header(arch):
  print("""\
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

#ifndef BERBERIS_GUEST_OS_PRIMITIVES_GEN_SYSCALL_NUMBERS_ARCH_H_
#define BERBERIS_GUEST_OS_PRIMITIVES_GEN_SYSCALL_NUMBERS_ARCH_H_

namespace berberis {""")


def _print_footer(arch):
  print("""\

}  // namespace berberis

#endif  // BERBERIS_GUEST_OS_PRIMITIVES_GEN_SYSCALL_NUMBERS_ARCH_H_""")


def _print_enum(arch, kernel_syscalls):
  print("""\

enum {""")

  for nr, syscall in sorted(kernel_syscalls.items()):
    if arch in syscall:
      assert nr.startswith('__')
      print('  GUEST_%s = %s,' % (nr[2:], syscall[arch]['id']))

  print('};')


def main(argv):
  src_arch = argv[1]
  dst_arch = argv[2]

  with open(argv[3]) as json_file:
    kernel_syscalls = json.load(json_file)

  # TODO(b/232598137): merge custom syscalls?

  display_src_arch = src_arch.upper()

  _print_header(display_src_arch)
  _print_enum(src_arch, kernel_syscalls)
  _print_footer(display_src_arch)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
