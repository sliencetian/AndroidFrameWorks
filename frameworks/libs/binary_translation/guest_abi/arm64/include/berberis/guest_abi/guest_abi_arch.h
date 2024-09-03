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
  // Currently we only support one calling covention for ARM64 but ARM have two and we may need to
  // support more than one in the future.
  enum CallingConventionsVariant { kAapcs64, kDefaultAbi = kAapcs64 };

 protected:
  enum class ArgumentClass { kInteger, kVFP, kLargeStructType };

  template <typename Type, typename = void>
  struct GuestArgumentInfo;

  template <typename IntegerType>
  struct GuestArgumentInfo<IntegerType, std::enable_if_t<std::is_integral_v<IntegerType>>> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = sizeof(IntegerType);
    // Use sizeof, not alignof for kAlignment because all integer types are naturally aligned on
    // ARM, which is not guaranteed to be true for host.
    constexpr static unsigned kAlignment = sizeof(IntegerType);
    using GuestType = GuestType<IntegerType>;
    using HostType = IntegerType;
  };

  template <typename EnumType>
  struct GuestArgumentInfo<EnumType, std::enable_if_t<std::is_enum_v<EnumType>>>
      : GuestArgumentInfo<std::underlying_type_t<EnumType>> {
    using GuestType = GuestType<EnumType>;
    using HostType = EnumType;
  };

  template <typename PointeeType>
  struct GuestArgumentInfo<PointeeType*> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = 8;
    constexpr static unsigned kAlignment = 8;
    using GuestType = GuestType<PointeeType*>;
    using HostType = PointeeType*;
  };

  template <typename ResultType, typename... ArgumentType>
  struct GuestArgumentInfo<ResultType (*)(ArgumentType...)> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kInteger;
    constexpr static unsigned kSize = 8;
    constexpr static unsigned kAlignment = 8;
    using GuestType = GuestType<ResultType (*)(ArgumentType...)>;
    using HostType = ResultType (*)(ArgumentType...);
  };

  template <>
  struct GuestArgumentInfo<float> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kVFP;
    constexpr static unsigned kSize = 4;
    constexpr static unsigned kAlignment = 4;
    // using GuestType = intrinsics::Float32;
    using GuestType = GuestType<float>;
    // using HostType = intrinsics::Float32;
    using HostType = float;
  };

  template <>
  struct GuestArgumentInfo<double> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kVFP;
    constexpr static unsigned kSize = 8;
    constexpr static unsigned kAlignment = 8;
    // using GuestType = intrinsics::Float64;
    using GuestType = GuestType<double>;
    // using HostType = intrinsics::Float64;
    using HostType = double;
  };

  template <typename LargeStructType>
  struct GuestArgumentInfo<
      LargeStructType,
      std::enable_if_t<std::is_class_v<LargeStructType> && (sizeof(LargeStructType) > 16)>> {
    constexpr static ArgumentClass kArgumentClass = ArgumentClass::kLargeStructType;
    // Note: when large structure is passed or returned it's kept in memory aalocated by caller
    // and pointer to it is passed instead.  We are describing that pointer below.
    constexpr static unsigned kSize = 8;
    constexpr static unsigned kAlignment = 8;
    // Note: despite the fact that we have pointer to structure passed, but actual structure we
    // keep information about underlying structure type here.
    // This is for passing structure as a function argument: we have to make it const to makes sure
    // it wouldn't be changed by accident and it's easier to do with this declaration.
    using GuestType = GuestType<LargeStructType>;
    using HostType = LargeStructType;
  };
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_ABI_GUEST_ABI_ARCH_H_
