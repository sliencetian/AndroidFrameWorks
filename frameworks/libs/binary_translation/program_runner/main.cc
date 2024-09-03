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

#include <malloc.h>
#include <sys/auxv.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "berberis/base/checks.h"

#if defined(__i386__) || defined(__x86_64__)
#include "berberis/program_runner/program_runner.h"
#elif defined(__aarch64__)
#include "berberis/guest_state/guest_state.h"
#include "berberis/interpreter/riscv64/interpreter.h"
#include "berberis/tiny_loader/loaded_elf_file.h"
#include "berberis/tiny_loader/tiny_loader.h"
#endif

// Program runner meant for testing and manual invocation.

namespace berberis {

namespace {

void Usage(const char* argv_0) {
  printf(
      "Usage: %s [-h|?] [-l loader] [-s vdso] guest_executable [arg1 [arg2 ...]]\n"
      "  -h, -?           - print this message\n"
      "  -l loader        - path to guest loader\n"
      "  -s vdso          - path to guest vdso\n"
      "  guest_executable - path to the guest executable\n",
      argv_0);
}

struct Options {
  const char* guest_executable;
  const char* loader_path;
  const char* vdso_path;
  bool print_help_and_exit;
};

Options ParseArgs(int argc, char* argv[]) {
  CHECK_GE(argc, 1);
  static const Options kOptsError{.print_help_and_exit = true};
  Options opts{
      .guest_executable = nullptr,
      .loader_path = nullptr,
      .vdso_path = nullptr,
      .print_help_and_exit = false,
  };

  int curr_arg = 1;
  for (int curr_arg = 1; curr_arg < argc; ++curr_arg) {
    if (argv[curr_arg][0] != '-') {
      break;
    }
    const char option = argv[curr_arg][1];
    switch (option) {
      case 's':
      case 'l':
        if (++curr_arg == argc) {
          return kOptsError;
        }
        if (option == 's') {
          opts.vdso_path = argv[curr_arg];
        } else {
          opts.loader_path = argv[curr_arg];
        }
        break;
      case 'h':
      case '?':
      default:
        return kOptsError;
    }
  }

  if (curr_arg >= argc) {
    return kOptsError;
  }

  return opts;
}

}  // namespace

}  // namespace berberis

int main(int argc, char* argv[], [[maybe_unused]] char* envp[]) {
#if defined(__GLIBC__)
  // Disable brk in glibc-malloc.
  //
  // By default GLIBC uses brk in malloc which may lead to conflicts with
  // executables that use brk for their own needs. See http://b/64720148 for
  // example.
  mallopt(M_MMAP_THRESHOLD, 0);
  mallopt(M_TRIM_THRESHOLD, -1);
#endif

  berberis::Options opts = berberis::ParseArgs(argc, argv);

  if (opts.print_help_and_exit) {
    berberis::Usage(argv[0]);
    return -1;
  }

  std::string error_msg;
#if defined(__i386__) || defined(__x86_64__) || defined(__riscv)
  if (!berberis::Run(
          // TODO(b/276787135): Make vdso and loader configurable via command line arguments.
          /* vdso_path */ nullptr,
          /* loader_path */ nullptr,
          argc - optind,
          const_cast<const char**>(argv + optind),
          envp,
          &error_msg)) {
    fprintf(stderr, "unable to start executable: %s\n", error_msg.c_str());
    return -1;
  }
#elif defined(__aarch64__)
  LoadedElfFile elf_file;
  if (!TinyLoader::LoadFromFile(argv[optind], &elf_file, &error_msg)) {
    fprintf(stderr, "unable to start load file: %s\n", error_msg.c_str());
    return -1;
  }
  if (elf_file.e_type() != ET_EXEC) {
    fprintf(stderr, "this is not a static executable file: %hu\n", elf_file.e_type());
    return -1;
  }

  berberis::ThreadState state{};
  state.cpu.insn_addr = berberis::ToGuestAddr(elf_file.entry_point());
  while (true) {
    InterpretInsn(&state);
  }
#else
#error Unsupported platform
#endif

  return 0;
}
