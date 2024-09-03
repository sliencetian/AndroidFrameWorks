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

#ifndef BERBERIS_INTRINSICS_INTRINSICS_BITMANIP_IMPL_H_
#define BERBERIS_INTRINSICS_INTRINSICS_BITMANIP_IMPL_H_

#include <cstdint>

namespace berberis::intrinsics {

// TODO(b/260725458): stop using __builtin_popcount after C++20 would become available.
template <>
inline std::tuple<int64_t> Cpop<int32_t, kUseCppImplementation>(int32_t src) {
  return {__builtin_popcount(src)};
}

// TODO(b/260725458): stop using __builtin_popcountll after C++20 would become available.
template <>
inline std::tuple<int64_t> Cpop<int64_t, kUseCppImplementation>(int64_t src) {
  return {__builtin_popcountll(src)};
}

template <typename ElementType>
std::tuple<ElementType> Brev8(ElementType arg) {
  constexpr unsigned long ls1 = 0x5555'5555'5555'5555;
  constexpr unsigned long rs1 = 0xAAAA'AAAA'AAAA'AAAA;
  constexpr unsigned long ls2 = 0x3333'3333'3333'3333;
  constexpr unsigned long rs2 = 0xCCCC'CCCC'CCCC'CCCC;
  constexpr unsigned long ls4 = 0x0F0F'0F0F'0F0F'0F0F;
  constexpr unsigned long rs4 = 0xF0F0'F0F0'F0F0'F0F0;
  auto tmp_arg = static_cast<typename ElementType::BaseType>(arg);

  tmp_arg = ((tmp_arg & ls1) << 1) | ((tmp_arg & rs1) >> 1);
  tmp_arg = ((tmp_arg & ls2) << 2) | ((tmp_arg & rs2) >> 2);
  tmp_arg = ((tmp_arg & ls4) << 4) | ((tmp_arg & rs4) >> 4);

  return {ElementType{tmp_arg}};
}

}  // namespace berberis::intrinsics

#endif  // BERBERIS_INTRINSICS_INTRINSICS_BITMANIP_IMPL_H_
