/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef BERBERIS_INTRINSICS_INTRINSICS_FLOATING_POINT_IMPL_H_
#define BERBERIS_INTRINSICS_INTRINSICS_FLOATING_POINT_IMPL_H_

#include <limits>
#include <tuple>
#include <type_traits>

#include "berberis/base/bit_util.h"
#include "berberis/intrinsics/guest_cpu_flags.h"
#include "berberis/intrinsics/intrinsics.h"
#include "berberis/intrinsics/intrinsics_float.h"  // Float32/Float64/ProcessNans
#include "berberis/intrinsics/type_traits.h"

namespace berberis::intrinsics {

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FAdd(int8_t rm, int8_t frm, FloatType arg1, FloatType arg2) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y) {
        return std::get<0>(FAddHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y));
      },
      arg1,
      arg2);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FAddHostRounding(FloatType arg1, FloatType arg2) {
  return {arg1 + arg2};
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<uint64_t> FClass(FloatType arg) {
  using IntType = std::make_unsigned_t<typename TypeTraits<FloatType>::Int>;
  constexpr IntType quiet_bit =
      __builtin_bit_cast(IntType, std::numeric_limits<FloatType>::quiet_NaN()) &
      ~__builtin_bit_cast(IntType, std::numeric_limits<FloatType>::signaling_NaN());
  const IntType raw_bits = bit_cast<IntType>(arg);

  switch (FPClassify(arg)) {
    case intrinsics::FPInfo::kNaN:
      return (raw_bits & quiet_bit) ? 0b10'0000'0000 : 0b01'0000'0000;
    case intrinsics::FPInfo::kInfinite:
      return intrinsics::SignBit(arg) ? 0b00'0000'0001 : 0b00'1000'0000;
    case intrinsics::FPInfo::kNormal:
      return intrinsics::SignBit(arg) ? 0b00'0000'0010 : 0b00'0100'0000;
    case intrinsics::FPInfo::kSubnormal:
      return intrinsics::SignBit(arg) ? 0b00'0000'0100 : 0b00'0010'0000;
    case intrinsics::FPInfo::kZero:
      return intrinsics::SignBit(arg) ? 0b00'0000'1000 : 0b00'0001'0000;
  }
}

template <typename TargetOperandType,
          typename SourceOperandType,
          enum PreferredIntrinsicsImplementation>
std::tuple<TargetOperandType> FCvtFloatToFloat(int8_t rm, int8_t frm, SourceOperandType arg) {
  static_assert(std::is_same_v<Float32, SourceOperandType> ||
                std::is_same_v<Float64, SourceOperandType>);
  static_assert(std::is_same_v<Float32, TargetOperandType> ||
                std::is_same_v<Float64, TargetOperandType>);
  if constexpr (sizeof(TargetOperandType) > sizeof(SourceOperandType)) {
    // Conversion from narrow type to wide one ignores rm because all possible values from narrow
    // type fit in the wide type.
    return TargetOperandType(arg);
  } else {
    return intrinsics::ExecuteFloatOperation<TargetOperandType>(
        rm, frm, [](auto x) { return typename TypeTraits<decltype(x)>::Narrow(x); }, arg);
  }
}

template <typename TargetOperandType,
          typename SourceOperandType,
          enum PreferredIntrinsicsImplementation>
std::tuple<TargetOperandType> FCvtFloatToInteger(int8_t rm, int8_t frm, SourceOperandType arg) {
  static_assert(std::is_same_v<Float32, SourceOperandType> ||
                std::is_same_v<Float64, SourceOperandType>);
  static_assert(std::is_integral_v<TargetOperandType>);
  int8_t actual_rm = rm == FPFlags::DYN ? frm : rm;
  SourceOperandType result = FPRound(arg, ToIntrinsicRoundingMode(actual_rm));
  if constexpr (std::is_signed_v<TargetOperandType>) {
    // Note: because of how two's complement numbers and floats work minimum negative number always
    // either representable precisely or not prepresentable at all, but this is not true for minimal
    // possible value.
    // Use ~min() to guarantee no surprises with rounding.
    constexpr float kMinInBoundsNegativeValue =
        static_cast<float>(std::numeric_limits<TargetOperandType>::min());
    constexpr float kMinNotInBoundsPositiveValue = static_cast<float>(-kMinInBoundsNegativeValue);
    if (result < SourceOperandType{kMinInBoundsNegativeValue}) [[unlikely]] {
      return std::numeric_limits<TargetOperandType>::min();
    }
    // Note: we have to ensure that NaN is properly handled by this comparison!
    if (result < SourceOperandType{kMinNotInBoundsPositiveValue}) [[likely]] {
      return static_cast<TargetOperandType>(result);
    }
  } else {
    // Note: if value is less than zero then result of conversion from float/double to unsigned
    // integer is undefined and thus clang/gcc happily use conversion cvttss2si without doing
    // anything to handle negative numbers.  We need to handle that corner case here.
    if (result < SourceOperandType{0.0f}) [[unlikely]] {
      return 0;
    }
    // Similarly to signed interners case above, have to use -2.0f * min to properly handle NaNs.
    constexpr float kMinNotInBoundsPositiveValue = static_cast<float>(
        -2.0f *
        static_cast<float>(std::numeric_limits<std::make_signed_t<TargetOperandType>>::min()));
    // Note: we have to ensure that NaN is properly handled by this comparison!
    if (result < SourceOperandType{kMinNotInBoundsPositiveValue}) [[likely]] {
      return static_cast<TargetOperandType>(result);
    }
  }
  // Handle too large numbers and NaN.
  return std::numeric_limits<TargetOperandType>::max();
}

template <typename TargetOperandType,
          typename SourceOperandType,
          enum PreferredIntrinsicsImplementation>
std::tuple<TargetOperandType> FCvtIntegerToFloat(int8_t /*rm*/,
                                                 int8_t /*frm*/,
                                                 SourceOperandType arg) {
  static_assert(std::is_integral_v<SourceOperandType>);
  static_assert(std::is_same_v<Float32, TargetOperandType> ||
                std::is_same_v<Float64, TargetOperandType>);
  // TODO(265372622): handle rm properly in integer-to-float and float-to-integer cases.
  TargetOperandType result = static_cast<TargetOperandType>(arg);
  return result;
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FDiv(int8_t rm, int8_t frm, FloatType arg1, FloatType arg2) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y) {
        return std::get<0>(FDivHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y));
      },
      arg1,
      arg2);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FDivHostRounding(FloatType arg1, FloatType arg2) {
  return {arg1 / arg2};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FMAdd(int8_t rm, int8_t frm, FloatType arg1, FloatType arg2, FloatType arg3) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y, auto z) {
        return std::get<0>(
            FMAddHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y, z));
      },
      arg1,
      arg2,
      arg3);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FMAddHostRounding(FloatType arg1, FloatType arg2, FloatType arg3) {
  return {intrinsics::MulAdd(arg1, arg2, arg3)};
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FMax(FloatType x, FloatType y) {
  return {Max(x, y)};
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FMin(FloatType x, FloatType y) {
  return {Min(x, y)};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FMSub(int8_t rm, int8_t frm, FloatType arg1, FloatType arg2, FloatType arg3) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y, auto z) {
        return std::get<0>(
            FMSubHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y, z));
      },
      arg1,
      arg2,
      arg3);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FMSubHostRounding(FloatType arg1, FloatType arg2, FloatType arg3) {
  return {intrinsics::MulAdd(arg1, arg2, intrinsics::Negative(arg3))};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FMul(int8_t rm, int8_t frm, FloatType arg1, FloatType arg2) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y) {
        return std::get<0>(FMulHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y));
      },
      arg1,
      arg2);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FMulHostRounding(FloatType arg1, FloatType arg2) {
  return {arg1 * arg2};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FNMAdd(int8_t rm,
                             int8_t frm,
                             FloatType arg1,
                             FloatType arg2,
                             FloatType arg3) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y, auto z) {
        return std::get<0>(
            FNMAddHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y, z));
      },
      arg1,
      arg2,
      arg3);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FNMAddHostRounding(FloatType arg1, FloatType arg2, FloatType arg3) {
  return {intrinsics::MulAdd(intrinsics::Negative(arg1), arg2, arg3)};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FNMSub(int8_t rm,
                             int8_t frm,
                             FloatType arg1,
                             FloatType arg2,
                             FloatType arg3) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y, auto z) {
        return std::get<0>(
            FNMSubHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y, z));
      },
      arg1,
      arg2,
      arg3);
}

template <typename FloatType>
FloatType CanonicalizeNanTuple(std::tuple<FloatType> arg) {
  return std::get<0>(CanonicalizeNan<FloatType>(std::get<0>(arg)));
}

template <typename FloatType>
FloatType RSqrtEstimate(FloatType op) {
  if (SignBit(op)) {
    // If argument is negative - return default NaN.
    return std::numeric_limits<FloatType>::quiet_NaN();
  }
  switch (FPClassify(op)) {
    case FPInfo::kNaN:
      // If argument is NaN - return default NaN.
      return std::numeric_limits<FloatType>::quiet_NaN();
    case FPInfo::kInfinite:
      return FloatType{0.0};
    case FPInfo::kSubnormal:
    case FPInfo::kZero:
      // If operand is too small - return the appropriate infinity.
      return CopySignBit(std::numeric_limits<FloatType>::infinity(), op);
    case FPInfo::kNormal:
      if constexpr (std::is_same_v<FloatType, Float32>) {
        uint32_t op_32 = bit_cast<uint32_t>(op);
        op_32 &= ~0xffff;
        op_32 += 0x8000;
        Float32 fp32 = bit_cast<Float32>(op_32);
        fp32 = (FloatType{1.0} / Sqrt(fp32));
        op_32 = bit_cast<uint32_t>(fp32);
        op_32 += 0x4000;
        op_32 &= ~0x7fff;
        return bit_cast<Float32>(op_32);
      } else {
        static_assert(std::is_same_v<FloatType, Float64>);
        uint64_t op_64 = bit_cast<uint64_t>(op);
        op_64 &= ~0x1fff'ffff'ffff;
        op_64 += 0x1000'0000'0000;
        Float64 fp64 = bit_cast<Float64>(op_64);
        fp64 = (FloatType{1.0} / Sqrt(fp64));
        op_64 = bit_cast<uint64_t>(fp64);
        op_64 += 0x0800'0000'0000;
        op_64 &= ~0x0fff'ffff'ffff;
        return bit_cast<Float64>(op_64);
      }
  }
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FNMSubHostRounding(FloatType arg1, FloatType arg2, FloatType arg3) {
  return {intrinsics::MulAdd(intrinsics::Negative(arg1), arg2, intrinsics::Negative(arg3))};
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FSgnj(FloatType x, FloatType y) {
  using Int = typename TypeTraits<FloatType>::Int;
  using UInt = std::make_unsigned_t<Int>;
  constexpr UInt sign_bit = std::numeric_limits<Int>::min();
  constexpr UInt non_sign_bit = std::numeric_limits<Int>::max();
  return {bit_cast<FloatType>((bit_cast<UInt>(x) & non_sign_bit) | (bit_cast<UInt>(y) & sign_bit))};
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FSgnjn(FloatType x, FloatType y) {
  return FSgnj(x, Negative(y));
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FSgnjx(FloatType x, FloatType y) {
  using Int = typename TypeTraits<FloatType>::Int;
  using UInt = std::make_unsigned_t<Int>;
  constexpr UInt sign_bit = std::numeric_limits<Int>::min();
  return {bit_cast<FloatType>(bit_cast<UInt>(x) ^ (bit_cast<UInt>(y) & sign_bit))};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FSqrt(int8_t rm, int8_t frm, FloatType arg) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x) {
        return std::get<0>(FSqrtHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x));
      },
      arg);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FSqrtHostRounding(FloatType arg) {
  return {Sqrt(arg)};
}

template <typename FloatType,
          enum PreferredIntrinsicsImplementation kPreferredIntrinsicsImplementation>
std::tuple<FloatType> FSub(int8_t rm, int8_t frm, FloatType arg1, FloatType arg2) {
  return intrinsics::ExecuteFloatOperation<FloatType>(
      rm,
      frm,
      [](auto x, auto y) {
        return std::get<0>(FSubHostRounding<decltype(x), kPreferredIntrinsicsImplementation>(x, y));
      },
      arg1,
      arg2);
}

template <typename FloatType, enum PreferredIntrinsicsImplementation>
std::tuple<FloatType> FSubHostRounding(FloatType arg1, FloatType arg2) {
  return {arg1 - arg2};
}

}  // namespace berberis::intrinsics

#endif  // BERBERIS_INTRINSICS_COMMON_INTRINSICS_H_
