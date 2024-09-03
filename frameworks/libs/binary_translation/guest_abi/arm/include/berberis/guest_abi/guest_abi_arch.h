/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef BERBERIS_GUEST_ABI_GUEST_ABI_ARCH_H_
#define BERBERIS_GUEST_ABI_GUEST_ABI_ARCH_H_

#include <array>

#include "berberis/guest_abi/guest_type.h"

namespace berberis {

class GuestAbi {
 public:
  enum CallingConventionsVariant { kAapcs, kAapcsVfp, kDefaultAbi = kAapcs };

 protected:
  enum class ArgumentClass { kInteger, kVFP, kReturnedViaIndirectPointer };

  template <typename Type, CallingConventionsVariant = kDefaultAbi, typename = void>
  struct GuestArgumentInfo;

  template <typename IntegerType, CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<
      IntegerType,
      kCallingConventionsVarіant,
      std::enable_if_t<std::is_integral_v<IntegerType> && std::is_signed_v<IntegerType> &&
                       sizeof(IntegerType) < 4>> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = sizeof(IntegerType);
    // Use sizeof, not alignof for kAlignment because all integer types are naturally aligned on
    // ARM, which is not guaranteed to be true for host.
    constexpr static unsigned kAlignment = sizeof(IntegerType);
    using GuestType = GuestType<int32_t>;
    using HostType = int32_t;
  };

  template <typename IntegerType, CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<
      IntegerType,
      kCallingConventionsVarіant,
      std::enable_if_t<std::is_integral_v<IntegerType> && !std::is_signed_v<IntegerType> &&
                       sizeof(IntegerType) < 4>> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = sizeof(IntegerType);
    // Use sizeof, not alignof for kAlignment because all integer types are naturally aligned on
    // ARM, which is not guaranteed to be true for host.
    constexpr static unsigned kAlignment = sizeof(IntegerType);
    using GuestType = GuestType<uint32_t>;
    using HostType = uint32_t;
  };

  template <typename IntegerType, CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<
      IntegerType,
      kCallingConventionsVarіant,
      std::enable_if_t<std::is_integral_v<IntegerType> && sizeof(IntegerType) >= 4>> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = sizeof(IntegerType);
    // Use sizeof, not alignof for kAlignment because all integer types are naturally aligned on
    // ARM, which is not guaranteed to be true for host.
    constexpr static unsigned kAlignment = sizeof(IntegerType);
    using GuestType = GuestType<IntegerType>;
    using HostType = IntegerType;
  };

  template <typename EnumType, CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<EnumType,
                           kCallingConventionsVarіant,
                           std::enable_if_t<std::is_enum_v<EnumType>>>
      : GuestArgumentInfo<std::underlying_type_t<EnumType>, kCallingConventionsVarіant> {
    using GuestType = GuestType<EnumType>;
    using HostType = EnumType;
  };

  template <typename PointeeType, CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<PointeeType*, kCallingConventionsVarіant> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = 4;
    constexpr static unsigned kAlignment = 4;
    using GuestType = GuestType<PointeeType*>;
    using HostType = PointeeType*;
  };

  template <typename ResultType,
            typename... ArgumentType,
            CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<ResultType (*)(ArgumentType...), kCallingConventionsVarіant> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = 4;
    constexpr static unsigned kAlignment = 4;
    using GuestType = GuestType<ResultType (*)(ArgumentType...)>;
    using HostType = ResultType (*)(ArgumentType...);
  };

  template <>
  struct GuestArgumentInfo<float, CallingConventionsVariant::kAapcs> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = 4;
    constexpr static unsigned kAlignment = 4;
    // using GuestType = intrinsics::Float32;
    using GuestType = GuestType<float>;
    // using HostType = intrinsics::Float32;
    using HostType = float;
  };

  template <>
  struct GuestArgumentInfo<float, CallingConventionsVariant::kAapcsVfp>
      : GuestArgumentInfo<float, CallingConventionsVariant::kAapcs> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kVFP;
  };

  template <>
  struct GuestArgumentInfo<double, CallingConventionsVariant::kAapcs> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = 8;
    constexpr static unsigned kAlignment = 8;
    constexpr static bool fp_argument = false;
    // using GuestType = intrinsics::Float64;
    using GuestType = GuestType<double>;
    // using HostType = intrinsics::Float64;
    using HostType = double;
  };

  template <>
  struct GuestArgumentInfo<double, CallingConventionsVariant::kAapcsVfp>
      : GuestArgumentInfo<double, CallingConventionsVariant::kAapcs> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kVFP;
  };

  template <typename LargeStructType, CallingConventionsVariant kCallingConventionsVarіant>
  struct GuestArgumentInfo<
      LargeStructType,
      kCallingConventionsVarіant,
      std::enable_if_t<std::is_class_v<LargeStructType> && (sizeof(LargeStructType) > 4)>> {
    // Note: this is a kludge for now.  When large structures are returned from function they are
    // passed via hidden first argument.  But when they are passed into function rules are quite
    // complicated — we don't support them yet.
    //
    // Attempt to use it as an argument of function would cause compile-time error thus we can be
    // sure this wouldn't affect us without us knowing it happened.
    //
    // Currently this class doesn't provide kSize and kAlignment members which means compilation
    // error would happen during construction of kArgumentsInfo array in the ArgumentsInfoHelper.
    //
    // If, in the future, such members would be added then another error would be detected during
    // processing of that array since ArgumentsInfoHelper explicitly parses all the possible values
    // of kArgumentClass which is can process.
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kReturnedViaIndirectPointer;
    using GuestType = GuestType<LargeStructType>;
    using HostType = LargeStructType;
  };
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_ABI_GUEST_ABI_ARCH_H_
