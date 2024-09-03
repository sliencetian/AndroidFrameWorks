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

#include <audio_utils/CommandThread.h>

#include <gtest/gtest.h>

TEST(commandthread, basic) {
    android::audio_utils::CommandThread ct;

    ct.add("one", [](){});
    ct.add("two", [](){});
    ct.quit();
    EXPECT_EQ(0, ct.size());
    EXPECT_EQ("", ct.dump());
}

TEST(commandthread, full) {
    std::mutex m;
    std::condition_variable cv;
    int stage = 0;
    android::audio_utils::CommandThread ct;

    // load the CommandThread queue.
    ct.add("one", [&]{
        std::unique_lock ul(m);
        stage = 1;
        cv.notify_one();
        cv.wait(ul, [&] { return stage == 2; });
    });
    ct.add("two", [&]{
        std::unique_lock ul(m);
        stage = 3;
        cv.notify_one();
        cv.wait(ul, [&] { return stage == 4; });
    });
    ct.add("three", [&]{
        std::unique_lock ul(m);
        stage = 5;
        cv.notify_one();
        cv.wait(ul, [&] { return stage == 6; });
    });

    std::unique_lock ul(m);

    // step through each command in the queue.

    cv.wait(ul, [&] { return stage == 1; });
    EXPECT_EQ(2, ct.size());
    EXPECT_EQ("two\nthree\n", ct.dump());
    stage = 2;
    cv.notify_one();

    cv.wait(ul, [&] { return stage == 3; });
    EXPECT_EQ(1, ct.size());
    EXPECT_EQ("three\n", ct.dump());
    stage = 4;
    cv.notify_one();

    cv.wait(ul, [&] { return stage == 5; });
    EXPECT_EQ(0, ct.size());
    EXPECT_EQ("", ct.dump());
    stage = 6;
    cv.notify_one();
}
