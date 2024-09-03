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

#include <audio_utils/float_test_utils.h>
#include <gtest/gtest.h>

TEST(audio_float_tests, Basic) {
    // Precision test for the CPU.
    // The ARM does subnormals but the x86-64 does not under fast-math.
    //
    // single precision float
    EXPECT_EQ(127, android::audio_utils::test::test_max_exponent<float>());

    // -126 is expected if subnormals are not implemented,
    // the value will be less than that if subnormals are implemented.
    EXPECT_LE(android::audio_utils::test::test_min_exponent<float>(), -126);

    EXPECT_EQ(23, android::audio_utils::test::test_mantissa<float>());

    // double precision float
    EXPECT_EQ(1023, android::audio_utils::test::test_max_exponent<double>());

    // -1022 is expected if subnormals are not implemented,
    // the value will be less than that if subnormals are implemented.
    EXPECT_LE(android::audio_utils::test::test_min_exponent<double>(), -1022);

    EXPECT_EQ(52, android::audio_utils::test::test_mantissa<double>());
}
