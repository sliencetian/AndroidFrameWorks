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

#include <cmath>
#include <functional>

namespace android::audio_utils::test {

/*
  Tests for float precision.

 ... w/o subnormals on an x86-64 -ffast-math -fhonor-infinities -fhonor-nans

 float32 without subnormals:
max_exponent: 127
min_exponent: -126
mantissa: 23

float64 without subnormals:
max_exponent: 1023
min_exponent: -1022
mantissa: 52

... with subnormals ARM and x86-64 normal compilation

float32 with subnormals:
max_exponent: 127
min_exponent: -149
mantissa: 23

float64 with subnormals:
max_exponent: 1023
min_exponent: -1074
mantissa: 52

*/

// This requires a functor that doubles the input.
template <typename D>
int test_max_exponent(const std::function<D(D)>& twice = [](D x) -> D { return x + x; }) {
    D d = 1;
    for (int i = 0; i < 16384; ++i) {
        d = twice(d);
        // std::cout << i << "  " << d << "\n";
        if (std::isinf(d)) {
            return i;
        }
    }
    return 0;
}

// This requires a functor that halves the input.
template <typename D>
int test_min_exponent(const std::function<D(D)>& half =[](D x) { return x * 0.5; } ) {
    D d = 1;
    for (int i = 0; i < 16384; ++i) {
        d = half(d);
        // std::cout << i << "  " << d << "\n";
        if (d == 0) {
            return -i;
        }
    }
    return 0;
}

// This requires a functor that increments the input.
template <typename D>
int test_mantissa(const std::function<D(D)>& inc = [](D x) { return x + 1; }) {
    D d = 1;
    for (int i = 0; i < 128; ++i) {
        d = d + d;
        // std::cout << i << "  " << d << "\n";
        if (d == inc(d)) {
            return i;
        }
    }
    return 0;
}

} // namespace android::audio_utils::test
