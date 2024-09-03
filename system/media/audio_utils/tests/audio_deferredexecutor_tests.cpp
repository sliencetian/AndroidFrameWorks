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

#include <audio_utils/DeferredExecutor.h>
#include <atomic>
#include <gtest/gtest.h>

// Test object
class RunOnClose {
public:
    template <typename F>
    explicit RunOnClose(F&& f) : thunk_(std::forward<F>(f)) {}
    explicit RunOnClose(RunOnClose&& other) = default;
    ~RunOnClose() { if (thunk_) thunk_(); }
private:
    std::function<void()> thunk_;
};

TEST(deferredexecutor, basic) {
    std::atomic<int> disposed{};
    std::atomic<int> deferred{};
    {
        android::audio_utils::DeferredExecutor de;

        de.defer([&](){++deferred;});
        de.dispose(std::make_shared<RunOnClose>([&]{++disposed;}));
        EXPECT_EQ(0, deferred);
        EXPECT_EQ(0, disposed);
        EXPECT_EQ(false, de.empty());
        de.process();
        EXPECT_EQ(1, deferred);
        EXPECT_EQ(1, disposed);
        EXPECT_EQ(true, de.empty());
    }
    EXPECT_EQ(1, deferred);
    EXPECT_EQ(1, disposed);
}

TEST(deferredexecutor, clear) {
    std::atomic<int> disposed{};
    std::atomic<int> deferred{};
    {
        android::audio_utils::DeferredExecutor de;

        de.defer([&](){++deferred;});
        de.dispose(std::make_shared<RunOnClose>([&]{++disposed;}));
        EXPECT_EQ(0, deferred);
        EXPECT_EQ(0, disposed);
        EXPECT_EQ(false, de.empty());
        de.clear();
        EXPECT_EQ(0, deferred);
        EXPECT_EQ(1, disposed);
        EXPECT_EQ(true, de.empty());
    }
    EXPECT_EQ(0, deferred);
    EXPECT_EQ(1, disposed);
}

class DtorAndRecursive : public testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(DtorAndRecursive, deferred_adds_deferred) {
    const auto [processInDtor, recursive] = GetParam();
    std::atomic<int> disposed{};
    std::atomic<int> deferred{};
    {
        android::audio_utils::DeferredExecutor de(processInDtor);

        // The deferred action adds another deferred action.
        de.defer([&](){ de.defer([&](){++deferred;}); ++deferred;});
        de.dispose(std::make_shared<RunOnClose>([&]{++disposed;}));
        EXPECT_EQ(0, deferred);
        EXPECT_EQ(0, disposed);
        EXPECT_EQ(false, de.empty());
        de.process(recursive);
        EXPECT_EQ(1 + recursive, deferred);
        EXPECT_EQ(1, disposed);
        EXPECT_EQ(recursive, de.empty());
    }
    EXPECT_EQ(1 + (recursive || processInDtor), deferred);
    EXPECT_EQ(1, disposed);
}

static const auto paramToString = [](const auto& param) {
    const auto [processInDtor, recursive] = param.param;
    return std::string("processInDtor_")
            .append(processInDtor ? "true" : "false")
            .append("__recursive_")
            .append(recursive ? "true" : "false");
};

INSTANTIATE_TEST_SUITE_P(DeferredExecutorSuite,
        DtorAndRecursive,
        testing::Combine(testing::Bool(), testing::Bool()),
        paramToString);
