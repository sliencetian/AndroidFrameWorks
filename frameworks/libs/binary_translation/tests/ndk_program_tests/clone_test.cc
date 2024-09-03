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

#include "gtest/gtest.h"

#include <linux/sched.h>
#include <sched.h>
#include <sys/wait.h>

#include <atomic>
#include <csignal>
#include <cstdlib>

#include "berberis/ndk_program_tests/scoped_sigaction.h"

namespace {

constexpr size_t kChildStack = 1024;

bool g_parent_handler_called;
bool g_child_handler_called;
bool g_grandchild_handler_called;

void VerifySignalHandler(bool* flag) {
  *flag = false;
  raise(SIGPWR);
  ASSERT_TRUE(*flag);
}

template <size_t kStackSize, typename Runner>
void CloneVMAndWait(Runner runner, int extra_flags, int expect_return) {
  void* child_stack[kStackSize];
  pid_t tid = clone(runner, &child_stack[kStackSize], CLONE_VM | extra_flags, nullptr);
  int status;
  ASSERT_EQ(tid, TEMP_FAILURE_RETRY(waitpid(tid, &status, __WCLONE)));
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), expect_return);
}

int SharedSighandRunner(void*) {
  // Grandchild shared handlers with child.
  VerifySignalHandler(&g_child_handler_called);
  struct sigaction sa {
    .sa_handler = +[](int) { g_grandchild_handler_called = true; }
  };
  // We intentionally do not restore sigaction to verify that this change
  // will also change the handler in child (parent of grandchild).
  EXPECT_EQ(sigaction(SIGPWR, &sa, nullptr), 0);
  VerifySignalHandler(&g_grandchild_handler_called);
  return 21;
}

int UnsharedSighandRunner(void*) {
  // Child inherits a copy of parent handlers.
  VerifySignalHandler(&g_parent_handler_called);
  struct sigaction sa {
    .sa_handler = +[](int) { g_child_handler_called = true; }
  };
  // We intentionally do not restore sigaction to verify that this change
  // doesn't affect signal handlers in parent.
  EXPECT_EQ(sigaction(SIGPWR, &sa, nullptr), 0);
  VerifySignalHandler(&g_child_handler_called);
  // Now clone with shared handlers.
  CloneVMAndWait<kChildStack>(SharedSighandRunner, CLONE_SIGHAND, 21);
  VerifySignalHandler(&g_grandchild_handler_called);
  return 42;
}

TEST(Clone, CloneVMSighandSharing) {
  struct sigaction sa {
    .sa_handler = +[](int) { g_parent_handler_called = true; }
  };
  ScopedSigaction scoped_sa(SIGPWR, &sa);
  // Clone a child with non-shared signal handlers.
  // Note that child's stack contains grandchild's stack, so should be larger.
  CloneVMAndWait<kChildStack * 2>(UnsharedSighandRunner, 0, 42);
  // Verify that children didn't alter parent's signal handlers.
  VerifySignalHandler(&g_parent_handler_called);
}

// We cannot accurately detect when grandchild stack can be free'd. So
// we just keep it in a global variable and never free.
void* g_grandchild_stack[kChildStack];
std::atomic<bool> g_child_finished;
std::atomic<bool> g_grandchild_finished;

int WaitUntilParentExitsAndVerifySignalHandlers(void*) {
  while (!g_child_finished) {
    sched_yield();
  }

  // Grandchild shares handlers with child and should still
  // be able to use them after child terminated.
  VerifySignalHandler(&g_child_handler_called);

  g_grandchild_finished = true;
  return 0;
}

int CloneOutlivingChild(void*) {
  struct sigaction sa {
    .sa_handler = +[](int) { g_child_handler_called = true; }
  };
  EXPECT_EQ(sigaction(SIGPWR, &sa, nullptr), 0);

  clone(WaitUntilParentExitsAndVerifySignalHandlers,
        &g_grandchild_stack[kChildStack],
        CLONE_VM | CLONE_SIGHAND,
        nullptr);
  return 42;
}

TEST(Clone, CloneVMChildOutlivingParent) {
  // We'll test grandchild outliving child.
  g_child_finished = false;
  g_grandchild_finished = false;

  CloneVMAndWait<kChildStack>(CloneOutlivingChild, 0, 42);

  g_child_finished = true;

  // Wait for grandchild to finish.
  while (!g_grandchild_finished) {
    sched_yield();
  }
}

}  // namespace
