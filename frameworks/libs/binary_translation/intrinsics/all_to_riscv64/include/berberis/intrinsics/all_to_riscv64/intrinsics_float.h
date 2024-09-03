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

#ifndef BERBERIS_INTRINSICS_ALL_TO_RISCV64_INTRINSICS_FLOAT_H_
#define BERBERIS_INTRINSICS_ALL_TO_RISCV64_INTRINSICS_FLOAT_H_

#include <cinttypes>
#include <cmath>

#include "berberis/base/bit_util.h"
#include "berberis/base/logging.h"
#include "berberis/intrinsics/common/intrinsics_float.h"  // Float32/Float64
#include "berberis/intrinsics/guest_rounding_modes.h"     // FE_HOSTROUND/FE_TIESAWAY

namespace berberis::intrinsics {

#define MAKE_BINARY_OPERATOR(guest_name, operator_name, assignment_name)                         \
                                                                                                 \
  inline Float32 operator operator_name(const Float32& v1, const Float32& v2) {                  \
    Float32 result;                                                                              \
    asm("f" #guest_name ".s %0, %1, %2" : "=f"(result.value_) : "f"(v1.value_), "f"(v2.value_)); \
    return result;                                                                               \
  }                                                                                              \
                                                                                                 \
  inline Float32& operator assignment_name(Float32 & v1, const Float32 & v2) {                   \
    asm("f" #guest_name ".s %0, %1, %2" : "=f"(v1.value_) : "f"(v1.value_), "f"(v2.value_));     \
    return v1;                                                                                   \
  }                                                                                              \
                                                                                                 \
  inline Float64 operator operator_name(const Float64& v1, const Float64& v2) {                  \
    Float64 result;                                                                              \
    asm("f" #guest_name ".d %0, %1, %2" : "=f"(result.value_) : "f"(v1.value_), "f"(v2.value_)); \
    return result;                                                                               \
  }                                                                                              \
                                                                                                 \
  inline Float64& operator assignment_name(Float64 & v1, const Float64 & v2) {                   \
    asm("f" #guest_name ".d %0, %1, %2" : "=f"(v1.value_) : "f"(v1.value_), "f"(v2.value_));     \
    return v1;                                                                                   \
  }

MAKE_BINARY_OPERATOR(add, +, +=)
MAKE_BINARY_OPERATOR(sub, -, -=)
MAKE_BINARY_OPERATOR(mul, *, *=)
MAKE_BINARY_OPERATOR(div, /, /=)

#undef MAKE_BINARY_OPERATOR

inline bool operator<(const Float32& v1, const Float32& v2) {
  bool result;
  asm("flt.s %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return result;
}

inline bool operator<(const Float64& v1, const Float64& v2) {
  bool result;
  asm("flt.d %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return result;
}

inline bool operator>(const Float32& v1, const Float32& v2) {
  bool result;
  asm("flt.s %0, %1, %2" : "=r"(result) : "f"(v2.value_), "f"(v1.value_));
  return result;
}

inline bool operator>(const Float64& v1, const Float64& v2) {
  bool result;
  asm("flt.d %0, %1, %2" : "=r"(result) : "f"(v2.value_), "f"(v1.value_));
  return result;
}

inline bool operator<=(const Float32& v1, const Float32& v2) {
  bool result;
  asm("fle.s %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return result;
}

inline bool operator<=(const Float64& v1, const Float64& v2) {
  bool result;
  asm("fle.d %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return result;
}

inline bool operator>=(const Float32& v1, const Float32& v2) {
  bool result;
  asm("fle.s %0, %1, %2" : "=r"(result) : "f"(v2.value_), "f"(v1.value_));
  return result;
}

inline bool operator>=(const Float64& v1, const Float64& v2) {
  bool result;
  asm("fle.d %0, %1, %2" : "=r"(result) : "f"(v2.value_), "f"(v1.value_));
  return result;
}

inline bool operator==(const Float32& v1, const Float32& v2) {
  bool result;
  asm("feq.s %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return result;
}

inline bool operator==(const Float64& v1, const Float64& v2) {
  bool result;
  asm("feq.d %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return result;
}

inline bool operator!=(const Float32& v1, const Float32& v2) {
  bool result;
  asm("feq.s %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return !result;
}

inline bool operator!=(const Float64& v1, const Float64& v2) {
  bool result;
  asm("feq.d %0, %1, %2" : "=r"(result) : "f"(v1.value_), "f"(v2.value_));
  return !result;
}

// It's NOT safe to use ANY functions which return float or double.  That's because IA32 ABI uses
// x87 stack to pass arguments (and does that even with -mfpmath=sse) and NaN float and
// double values would be corrupted if pushed on it.

inline Float32 Negative(const Float32& v) {
  Float32 result;
  asm("fneg.s %0, %1" : "=f"(result.value_) : "f"(v.value_));
  return result;
}

inline Float64 Negative(const Float64& v) {
  Float64 result;
  asm("fneg.d %0, %1" : "=f"(result.value_) : "f"(v.value_));
  return result;
}

inline Float32 FPRound(const Float32& value, uint32_t /*round_control*/) {
  LOG_ALWAYS_FATAL("Unimplemented for riscv!");
  return value;
}

inline Float64 FPRound(const Float64& value, uint32_t /*round_control*/) {
  LOG_ALWAYS_FATAL("Unimplemented for riscv!");
  return value;
}

}  // namespace berberis::intrinsics

#endif  // BERBERIS_INTRINSICS_ALL_TO_RISCV64_INTRINSICS_FLOAT_H_
