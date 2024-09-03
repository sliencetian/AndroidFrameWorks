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

#ifndef BERBERIS_GUEST_ABI_GUEST_ARGUMENTS_ARCH_H_
#define BERBERIS_GUEST_ABI_GUEST_ARGUMENTS_ARCH_H_

#include <array>

#include "berberis/base/dependent_false.h"
#include "berberis/calling_conventions/calling_conventions_arm64.h"
#include "berberis/guest_abi/guest_abi.h"

namespace berberis {

struct GuestArgumentBuffer {
  int argc;        // in general registers
  int resc;        // in general registers
  int simd_argc;   // in simd registers
  int simd_resc;   // in simd registers
  int stack_argc;  // in bytes

  // Basically a quote from GuestState.
  uint64_t argv[8];
  __uint128_t simd_argv[8];
  uint64_t stack_argv[1];  // VLA
};

template <typename, GuestAbi::CallingConventionsVariant = GuestAbi::kAapcs64>
class GuestArgumentsAndResult;

// GuestArguments is typesafe wrapper around GuestArgumentBuffer.
// Usage looks like this:
//   GuestArguments<double(int, double, int, double)> args(*buf);
//   int x = args.Arguments<0>();
//   float y = args.Arguments<1>();
//   args.Result() = x * y;

template <typename ResultType, typename... ArgumentType, bool kNoexcept>
class GuestArgumentsAndResult<ResultType(ArgumentType...) noexcept(kNoexcept), GuestAbi::kAapcs64>
    : GuestAbi {
 public:
  GuestArgumentsAndResult(GuestArgumentBuffer* buffer) : buffer_(buffer) {}

  template <size_t index>
  auto& GuestArgument() const {
    static_assert(index < sizeof...(ArgumentType));
    using Type = std::tuple_element_t<index, std::tuple<ArgumentType...>>;
    using GuestType = typename GuestArgumentInfo<Type>::GuestType;
    if constexpr (GuestArgumentInfo<Type>::kArgumentClass == ArgumentClass::kLargeStructType) {
      return **reinterpret_cast<const GuestType**>(ArgLocationAddress(kArgumentsLocations[index]));
    } else {
      return *reinterpret_cast<GuestType*>(ArgLocationAddress(kArgumentsLocations[index]));
    }
  }

  template <size_t index>
  auto& HostArgument() const {
    static_assert(index < sizeof...(ArgumentType));
    using Type = std::tuple_element_t<index, std::tuple<ArgumentType...>>;
    using HostType = typename GuestArgumentInfo<Type>::HostType;
    if constexpr (GuestArgumentInfo<Type>::kArgumentClass == ArgumentClass::kLargeStructType) {
      return **reinterpret_cast<const HostType**>(ArgLocationAddress(kArgumentsLocations[index]));
    } else {
      return *reinterpret_cast<HostType*>(ArgLocationAddress(kArgumentsLocations[index]));
    }
  }

  auto& GuestResult() const {
    static_assert(!std::is_same_v<ResultType, void>);
    if constexpr (GuestArgumentInfo<ResultType>::kArgumentClass ==
                  ArgumentClass::kLargeStructType) {
      return **reinterpret_cast<typename GuestArgumentInfo<ResultType>::GuestType**>(
          ArgLocationAddress(kResultLocation));
    } else {
      return *reinterpret_cast<typename GuestArgumentInfo<ResultType>::GuestType*>(
          ArgLocationAddress(kResultLocation));
    }
  }

  auto& HostResult() const {
    static_assert(!std::is_same_v<ResultType, void>);
    if constexpr (GuestArgumentInfo<ResultType>::kArgumentClass ==
                  ArgumentClass::kLargeStructType) {
      return **reinterpret_cast<typename GuestArgumentInfo<ResultType>::HostType**>(
          ArgLocationAddress(kResultLocation));
    } else {
      return *reinterpret_cast<typename GuestArgumentInfo<ResultType>::HostType*>(
          ArgLocationAddress(kResultLocation));
    }
  }

 private:
  constexpr static const std::array<arm64::ArgLocation, sizeof...(ArgumentType)>
  ArgumentsInfoHelper() {
    struct {
      const ArgumentClass kArgumentClass;
      const unsigned kSize;
      const unsigned kAlignment;
    } const kArgumentsInfo[] = {{.kArgumentClass = GuestArgumentInfo<ArgumentType>::kArgumentClass,
                                 .kSize = GuestArgumentInfo<ArgumentType>::kSize,
                                 .kAlignment = GuestArgumentInfo<ArgumentType>::kAlignment}...};

    arm64::CallingConventions conv;
    std::array<arm64::ArgLocation, sizeof...(ArgumentType)> result{};
    for (const auto& kArgInfo : kArgumentsInfo) {
      if (kArgInfo.kArgumentClass == ArgumentClass::kInteger ||
          kArgInfo.kArgumentClass == ArgumentClass::kLargeStructType) {
        result[&kArgInfo - kArgumentsInfo] =
            conv.GetNextIntArgLoc(kArgInfo.kSize, kArgInfo.kAlignment);
      } else if (kArgInfo.kArgumentClass == ArgumentClass::kVFP) {
        result[&kArgInfo - kArgumentsInfo] =
            conv.GetNextFpArgLoc(kArgInfo.kSize, kArgInfo.kAlignment);
      } else {
        LOG_ALWAYS_FATAL("Unsupported ArgumentClass");
      }
    }

    return result;
  }

  constexpr static arm64::ArgLocation ResultInfoHelper() {
    arm64::CallingConventions conv;
    using ResultInfo = GuestArgumentInfo<ResultType>;
    if constexpr (std::is_same_v<ResultType, void>) {
      return {arm64::kArgLocationNone, 0};
    } else if constexpr (ResultInfo::kArgumentClass == ArgumentClass::kInteger) {
      return conv.GetIntResLoc(ResultInfo::kSize);
    } else if constexpr (ResultInfo::kArgumentClass == ArgumentClass::kVFP) {
      return conv.GetFpResLoc(ResultInfo::kSize);
    } else {
      static_assert(kDependentTypeFalse<ResultType>, "Unsupported ArgumentClass");
    }
  }

  constexpr void* ArgLocationAddress(arm64::ArgLocation loc) const {
    if (loc.kind == arm64::kArgLocationStack) {
      return reinterpret_cast<char*>(buffer_->stack_argv) + loc.offset;
    } else if (loc.kind == arm64::kArgLocationInt) {
      return buffer_->argv + loc.offset;
    } else if (loc.kind == arm64::kArgLocationSimd) {
      return buffer_->simd_argv + loc.offset;
    } else {
      CHECK(false);
    }
  }

  constexpr static arm64::ArgLocation kResultLocation = ResultInfoHelper();

  constexpr static std::array<arm64::ArgLocation, sizeof...(ArgumentType)> kArgumentsLocations =
      ArgumentsInfoHelper();

  GuestArgumentBuffer* const buffer_;
};

// Partial specialization for GuestArgumentsAndResult<FunctionToPointer> - it acts the same
// as the corresponding GuestArgumentsAndResult<Function>.
template <typename ResultType, typename... ArgumentType, bool kNoexcept>
class GuestArgumentsAndResult<ResultType (*)(ArgumentType...) noexcept(kNoexcept),
                              GuestAbi::kAapcs64>
    : public GuestArgumentsAndResult<ResultType(ArgumentType...) noexcept(kNoexcept),
                                     GuestAbi::kAapcs64> {
 public:
  GuestArgumentsAndResult(GuestArgumentBuffer* buffer)
      : GuestArgumentsAndResult<ResultType(ArgumentType...) noexcept(kNoexcept),
                                GuestAbi::kAapcs64>(buffer) {}
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_ABI_GUEST_ARGUMENTS_ARCH_H_
