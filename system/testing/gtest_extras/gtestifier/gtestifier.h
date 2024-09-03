// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file is included when gtestifier.py rewrites a source file for a standalone test that is
// normally run as main() to turn it into a gtest.

#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#define GTESTIFIER_STRINGIFY(x) GTESTIFIER_STRINGIFY2(x)
#define GTESTIFIER_STRINGIFY2(x) #x

#define GTESTIFIER_CONCAT(x, y) GTESTIFIER_CONCAT2(x, y)
#define GTESTIFIER_CONCAT2(x, y) x##y

// Create unique names for main() and gtestifierWrapper() so that multiple standalone tests
// can be linked together.
#define main GTESTIFIER_CONCAT(GTESTIFIER_TEST, _main)
#define gtestifierWrapper GTESTIFIER_CONCAT(GTESTIFIER_TEST, _wrapper)

__BEGIN_DECLS
void registerGTestifierTest(const char* test_suite_name, const char* test_name, const char* file,
                            int line, int (*func)(), bool (*predicate)(int));

// The signature of main() needs to match the definition used in the standalone test.  If
// the standalone test uses main() with no arguments then the code will need to be compiled
// with -DGTESTIFIER_MAIN_NO_ARGUMENTS.
#if GTESTIFIER_MAIN_NO_ARGUMENTS
int main(void);
#else
int main(int argc, char** argv);
#endif
__END_DECLS

// gtestifierWrapper wraps the standalone main() function.
static int gtestifierWrapper(void) {
#if GTESTIFIER_MAIN_NO_ARGUMENTS
  return main();
#else
  char* argv[] = {strdup(GTESTIFIER_STRINGIFY(GTESTIFIER_TEST)), NULL};
  return main(1, argv);
#endif
}

// Use a static constructor to register this wrapped test as a gtest.
__attribute__((constructor)) static void registerGTestifier() {
  registerGTestifierTest(GTESTIFIER_STRINGIFY(GTESTIFIER_SUITE),
                         GTESTIFIER_STRINGIFY(GTESTIFIER_TEST), __FILE__, __LINE__,
                         gtestifierWrapper, GTESTIFIER_PREDICATE);
};
