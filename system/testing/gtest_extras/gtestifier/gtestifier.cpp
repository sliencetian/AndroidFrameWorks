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

#include <gtest/gtest.h>

class GTestifierTest : public testing::Test {
 public:
  GTestifierTest(std::function<int()> func, std::function<bool(int)> predicate,
                 std::string test_name)
      : child_test_(func), predicate_(predicate), test_name_(test_name) {}
  void TestBody() {
    int result = child_test_();
    bool pass = predicate_ ? predicate_(result) : result == 0;
    if (!pass) {
      FAIL() << "Test " << test_name_ << " failed, result " << result;
    }
  }

 private:
  std::function<int()> child_test_;
  std::function<bool(int)> predicate_;
  std::string test_name_;
};

extern "C" void registerGTestifierTest(const char* test_suite_name, const char* test_name,
                                       const char* file, int line, int (*func)(),
                                       bool (*predicate)(int)) {
  testing::RegisterTest(
      test_suite_name, test_name, nullptr, nullptr, file, line,
      [=]() -> GTestifierTest* { return new GTestifierTest(func, predicate, test_name); });
}
