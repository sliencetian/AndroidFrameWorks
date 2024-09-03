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
#include "berberis/calling_conventions/calling_conventions_arm.h"
#include "berberis/guest_abi/guest_abi.h"

namespace berberis {

// Args come in packed in 32-bit slots array 'argv' according to ARM rules,
// 'argc' is a number of args slots.
// Result goes out packed in 'argv' according to ARM rules,
// 'resc' is a number of result slots.
// Why extra copy args?
// To get current thread state and stack, we need to call C function. That
// means we have to save all original register args first.
// Why extra copy result?
// If we detach thread right after guest call, thread state gets deleted.
// That means we have to copy result out of thread state.
struct GuestArgumentBuffer {
  int argc;          // in 4-byte slots
  int resc;          // in 4-byte slots
  uint32_t argv[1];  // VLA
};

template <typename, GuestAbi::CallingConventionsVariant = GuestAbi::kAapcs>
class GuestArgumentsAndResult;

// GuestArgumentsAndResult is typesafe wrapper around GuestArgumentBuffer.
// Usage looks like this:
//   GuestArgumentsAndResult<double(int, double, int, double)> args(buf);
//   int x = args.Arguments<0>();
//   float y = args.Arguments<1>();
//   args.Result() = x * y;

template <typename ResultType,
          typename... ArgumentType,
          bool kNoexcept,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
class GuestArgumentsAndResult<ResultType(ArgumentType...) noexcept(kNoexcept),
                              kCallingConventionsVariant> : GuestAbi {
 public:
  GuestArgumentsAndResult(GuestArgumentBuffer* buffer) : buffer_(buffer) {}

  template <size_t index>
  auto& GuestArgument() const {
    static_assert(index < sizeof...(ArgumentType));
    using Type =
        typename GuestArgumentInfo<std::tuple_element_t<index, std::tuple<ArgumentType...>>,
                                   kCallingConventionsVariant>::GuestType;
    return *reinterpret_cast<Type*>(ArgLocationAddress(kArgumentsLocations[index]));
  }

  template <size_t index>
  auto& HostArgument() const {
    static_assert(index < sizeof...(ArgumentType));
    using Type =
        typename GuestArgumentInfo<std::tuple_element_t<index, std::tuple<ArgumentType...>>,
                                   kCallingConventionsVariant>::HostType;
    return *reinterpret_cast<Type*>(ArgLocationAddress(kArgumentsLocations[index]));
  }

  auto& GuestResult() const {
    static_assert(!std::is_same_v<ResultType, void>);
    if constexpr (GuestArgumentInfo<ResultType, kCallingConventionsVariant>::kArgumentClass ==
                  ArgumentClass::kReturnedViaIndirectPointer) {
      return **reinterpret_cast<
          typename GuestArgumentInfo<ResultType, kCallingConventionsVariant>::GuestType**>(
          ArgLocationAddress(kResultLocation));
    } else {
      return *reinterpret_cast<
          typename GuestArgumentInfo<ResultType, kCallingConventionsVariant>::GuestType*>(
          ArgLocationAddress(kResultLocation));
    }
  }

  auto& HostResult() const {
    static_assert(!std::is_same_v<ResultType, void>);
    if constexpr (GuestArgumentInfo<ResultType, kCallingConventionsVariant>::kArgumentClass ==
                  ArgumentClass::kReturnedViaIndirectPointer) {
      return **reinterpret_cast<
          typename GuestArgumentInfo<ResultType, kCallingConventionsVariant>::HostType**>(
          ArgLocationAddress(kResultLocation));
    } else {
      return *reinterpret_cast<
          typename GuestArgumentInfo<ResultType, kCallingConventionsVariant>::HostType*>(
          ArgLocationAddress(kResultLocation));
    }
  }

 private:
  constexpr static const std::array<arm::ArgLocation, sizeof...(ArgumentType)>
  ArgumentsInfoHelper() {
    struct {
      const ArgumentClass kArgumentClass;
      const unsigned kSize;
      const unsigned kAlignment;
    } const kArgumentsInfo[] = {
        {.kArgumentClass =
             GuestArgumentInfo<ArgumentType, kCallingConventionsVariant>::kArgumentClass,
         .kSize = GuestArgumentInfo<ArgumentType, kCallingConventionsVariant>::kSize,
         .kAlignment = GuestArgumentInfo<ArgumentType, kCallingConventionsVariant>::kAlignment}...};

    arm::CallingConventions conv;

    // Skip hidden argument if it exists.
    if constexpr (!std::is_same_v<ResultType, void>) {
      if constexpr (GuestArgumentInfo<ResultType, kCallingConventionsVariant>::kArgumentClass ==
                    ArgumentClass::kReturnedViaIndirectPointer) {
        static_assert(GuestArgumentInfo<ResultType*, kCallingConventionsVariant>::kArgumentClass ==
                      ArgumentClass::kInteger);
        conv.GetNextIntArgLoc(
            GuestArgumentInfo<ResultType*, kCallingConventionsVariant>::kSize,
            GuestArgumentInfo<ResultType*, kCallingConventionsVariant>::kAlignment);
      }
    }

    std::array<arm::ArgLocation, sizeof...(ArgumentType)> result{};
    for (const auto& kArgInfo : kArgumentsInfo) {
      if (kArgInfo.kArgumentClass == ArgumentClass::kInteger) {
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

  constexpr static arm::ArgLocation ResultInfoHelper() {
    arm::CallingConventions conv;
    using ResultInfo = GuestArgumentInfo<ResultType, kCallingConventionsVariant>;
    if constexpr (std::is_same_v<ResultType, void>) {
      return {arm::kArgLocationNone, 0};
    } else if constexpr (ResultInfo::kArgumentClass == ArgumentClass::kInteger) {
      return conv.GetIntResLoc(ResultInfo::kSize);
    } else if constexpr (ResultInfo::kArgumentClass == ArgumentClass::kVFP) {
      return conv.GetFpResLoc(ResultInfo::kSize);
    } else if constexpr (ResultInfo::kArgumentClass == ArgumentClass::kReturnedViaIndirectPointer) {
      static_assert(GuestArgumentInfo<ResultType*, kCallingConventionsVariant>::kArgumentClass ==
                    ArgumentClass::kInteger);
      return conv.GetIntResLoc(ResultInfo::kSize);
    } else {
      static_assert(kDependentTypeFalse<ResultType>, "Unsupported ArgumentClass");
    }
  }

  constexpr void* ArgLocationAddress(arm::ArgLocation loc) const {
    if (loc.kind == arm::kArgLocationStack) {
      CHECK_EQ(loc.offset % 4, 0);
      return buffer_->argv + loc.offset / 4 + 4 /* 4 integer registers before stack */;
    } else if (loc.kind == arm::kArgLocationInt || loc.kind == arm::kArgLocationIntAndStack) {
      return buffer_->argv + loc.offset;
    } else if (loc.kind == arm::kArgLocationSimd) {
      CHECK(false);  // Temporary till we would support Aapcs-Vfp.
    } else {
      CHECK(false);
    }
  }

  constexpr static arm::ArgLocation kResultLocation = ResultInfoHelper();

  constexpr static std::array<arm::ArgLocation, sizeof...(ArgumentType)> kArgumentsLocations =
      ArgumentsInfoHelper();

  GuestArgumentBuffer* const buffer_;
};

// Partial specialization for GuestArgumentsAndResult<FunctionToPointer> - it acts the same
// as the corresponding GuestArgumentsAndResult<Function>.
template <typename ResultType,
          typename... ArgumentType,
          bool kNoexcept,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
class GuestArgumentsAndResult<ResultType (*)(ArgumentType...) noexcept(kNoexcept),
                              kCallingConventionsVariant>
    : public GuestArgumentsAndResult<ResultType(ArgumentType...) noexcept(kNoexcept),
                                     kCallingConventionsVariant> {
 public:
  GuestArgumentsAndResult(GuestArgumentBuffer* buffer)
      : GuestArgumentsAndResult<ResultType(ArgumentType...) noexcept(kNoexcept),
                                kCallingConventionsVariant>(buffer) {}
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_ABI_GUEST_ARGUMENTS_ARCH_H_
