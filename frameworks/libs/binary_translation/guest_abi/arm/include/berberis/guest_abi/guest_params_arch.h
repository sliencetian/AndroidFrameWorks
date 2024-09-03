/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef BERBERIS_GUEST_ABI_GUEST_PARAMS_ARCH_H_
#define BERBERIS_GUEST_ABI_GUEST_PARAMS_ARCH_H_

#include <array>
#include <cstdint>
#include <type_traits>

#include "berberis/base/bit_util.h"
#include "berberis/base/dependent_false.h"
#include "berberis/calling_conventions/calling_conventions_arm.h"
#include "berberis/guest_abi/guest_abi.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

class GuestVAListParams;

class GuestParamsAndReturnHelper : protected GuestAbi {
 protected:
  template <typename Type,
            GuestAbi::CallingConventionsVariant kCallingConventionsVariant,
            typename R,
            typename D,
            typename S>
  constexpr static auto* ParamLocationAddress(R r, D d, S s, arm::ArgLocation loc) {
    void* address = nullptr;
    if (loc.kind == arm::kArgLocationStack) {
      if constexpr (std::is_same_v<S, std::nullptr_t>) {
        address = ToHostAddr<uint32_t>(loc.offset);
      } else {
        address = s + loc.offset;
      }
    } else if (loc.kind == arm::kArgLocationInt) {
      if constexpr (std::is_same_v<R, std::nullptr_t>) {
        LOG_ALWAYS_FATAL("Unsupported ArgumentKind");
      } else {
        address = r + loc.offset;
      }
    } else if (loc.kind == arm::kArgLocationSimd) {
      if constexpr (std::is_same_v<D, std::nullptr_t>) {
        LOG_ALWAYS_FATAL("Unsupported ArgumentKind");
      } else {
        address = d + loc.offset;
      }
    } else if (loc.kind == arm::kArgLocationIntAndStack) {
      LOG_ALWAYS_FATAL("Arguments split between registers are stack are not currently supported");
    } else {
      LOG_ALWAYS_FATAL("Unknown ArgumentKind");
    }

    static_assert(!std::is_same_v<Type, void>);
    if constexpr (GuestArgumentInfo<Type, kCallingConventionsVariant>::kArgumentClass ==
                  ArgumentClass::kReturnedViaIndirectPointer) {
      return *reinterpret_cast<
          typename GuestArgumentInfo<Type, kCallingConventionsVariant>::GuestType**>(address);
    } else {
      return reinterpret_cast<
          typename GuestArgumentInfo<Type, kCallingConventionsVariant>::GuestType*>(address);
    }
  }
};

// GuestParamsAndReturn is typesafe accessor into ThreadState.
// Usage looks like this:
//   GuestParamsAndReturn<double(int, double, int, double)> params(state);
//   auto x = params.Params<0>();
//   auto y = params.Params<1>();
//   params.Return() = x * y;

template <typename, GuestAbi::CallingConventionsVariant = GuestAbi::kAapcs>
class GuestParamsAndReturn;  // Not defined.

template <typename ReturnType,
          typename... ParamType,
          bool kNoexcept,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
class GuestParamsAndReturn<ReturnType(ParamType...) noexcept(kNoexcept), kCallingConventionsVariant>
    : GuestParamsAndReturnHelper {
 public:
  GuestParamsAndReturn(ThreadState* state)
      : r_(reinterpret_cast<uint32_t*>(state->cpu.r)),
        d_(reinterpret_cast<uint32_t*>(state->cpu.d)),
        s_(ToHostAddr<uint8_t>(state->cpu.r[13])) {}

  template <size_t index>
  auto* Params() const {
    static_assert(index < sizeof...(ParamType));
    return this->ParamLocationAddress<std::tuple_element_t<index, std::tuple<ParamType...>>,
                                      kCallingConventionsVariant>(
        r_, d_, s_, kParamsLocations[index]);
  }

  auto* Return() const {
    return this->ParamLocationAddress<ReturnType, kCallingConventionsVariant>(
        r_, d_, s_, kReturnLocation);
  }

 private:
  friend class GuestVAListParams;
  friend class GuestParamsAndReturn<ReturnType(ParamType..., ...) noexcept(kNoexcept),
                                    kCallingConventionsVariant>;

  constexpr static const std::tuple<arm::CallingConventions,
                                    std::array<arm::ArgLocation, sizeof...(ParamType)>>
  ParamsInfoHelper() {
    struct {
      const ArgumentClass kArgumentClass;
      const unsigned kSize;
      const unsigned kAlignment;
    } const kArgumentsInfo[] = {
        {.kArgumentClass = GuestArgumentInfo<ParamType, kCallingConventionsVariant>::kArgumentClass,
         .kSize = GuestArgumentInfo<ParamType, kCallingConventionsVariant>::kSize,
         .kAlignment = GuestArgumentInfo<ParamType, kCallingConventionsVariant>::kAlignment}...};

    arm::CallingConventions conv;

    // Skip hidden argument if it exists.
    if constexpr (!std::is_same_v<ReturnType, void>) {
      if constexpr (GuestArgumentInfo<ReturnType, kCallingConventionsVariant>::kArgumentClass ==
                    ArgumentClass::kReturnedViaIndirectPointer) {
        conv.GetNextIntArgLoc(
            GuestArgumentInfo<ReturnType*, kCallingConventionsVariant>::kSize,
            GuestArgumentInfo<ReturnType*, kCallingConventionsVariant>::kAlignment);
      }
    }

    std::array<arm::ArgLocation, sizeof...(ParamType)> result{};
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

    return {conv, result};
  }

  constexpr static arm::ArgLocation ReturnInfoHelper() {
    arm::CallingConventions conv;
    using ReturnInfo = GuestArgumentInfo<ReturnType, kCallingConventionsVariant>;
    if constexpr (std::is_same_v<ReturnType, void>) {
      return {arm::kArgLocationNone, 0};
    } else if constexpr (ReturnInfo::kArgumentClass == ArgumentClass::kInteger) {
      return conv.GetIntResLoc(ReturnInfo::kSize);
    } else if constexpr (ReturnInfo::kArgumentClass == ArgumentClass::kVFP) {
      return conv.GetFpResLoc(ReturnInfo::kSize);
    } else if constexpr (ReturnInfo::kArgumentClass == ArgumentClass::kReturnedViaIndirectPointer) {
      static_assert(GuestArgumentInfo<ReturnType*, kCallingConventionsVariant>::kArgumentClass ==
                    ArgumentClass::kInteger);
      return conv.GetIntResLoc(GuestArgumentInfo<ReturnType*, kCallingConventionsVariant>::kSize);
    } else {
      static_assert(kDependentTypeFalse<ReturnType>, "Unsupported ArgumentClass");
    }
  }

  constexpr static std::array<arm::ArgLocation, sizeof...(ParamType)> kParamsLocations =
      std::get<1>(ParamsInfoHelper());

  constexpr static arm::ArgLocation kReturnLocation = ReturnInfoHelper();

  uint32_t* r_;
  uint32_t* d_;
  uint8_t* s_;
};

// Partial specialization for GuestParamsAndReturn<FunctionToPointer> - it acts the same
// as the corresponding GuestParamsAndReturn<Function>.
template <typename ReturnType,
          typename... ParamType,
          bool kNoexcept,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
class GuestParamsAndReturn<ReturnType (*)(ParamType...) noexcept(kNoexcept),
                           kCallingConventionsVariant>
    : public GuestParamsAndReturn<ReturnType(ParamType...) noexcept(kNoexcept),
                                  kCallingConventionsVariant> {
 public:
  GuestParamsAndReturn(ThreadState* state)
      : GuestParamsAndReturn<ReturnType(ParamType...) noexcept(kNoexcept),
                             kCallingConventionsVariant>(state) {}
};

// Partial specialization for GuestParamsAndReturn<Function> for functions with variadic
// arguents - it acts the same as the corresponding one without it (but with forced kAapcs ABI).
// “Note: There are no VFP CPRCs in a variadic procedure” ⇦ from AAPCS
template <typename ReturnType,
          typename... ParamType,
          bool kNoexcept,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
class GuestParamsAndReturn<ReturnType(ParamType..., ...) noexcept(kNoexcept),
                           kCallingConventionsVariant>
    : public GuestParamsAndReturn<ReturnType(ParamType...) noexcept(kNoexcept),
                                  GuestAbi::kDefaultAbi> {
 public:
  GuestParamsAndReturn(ThreadState* state)
      : GuestParamsAndReturn<ReturnType(ParamType...) noexcept(kNoexcept), GuestAbi::kDefaultAbi>(
            state) {}

 private:
  friend class GuestVAListParams;
  constexpr static arm::CallingConventions kVAStartBase =
      std::get<0>(GuestParamsAndReturn<ReturnType(ParamType...) noexcept(kNoexcept),
                                       GuestAbi::kDefaultAbi>::ParamsInfoHelper());
};

// Partial specialization for GuestParamsAndReturn<FunctionToPointer> for functions with
// variadic arguents - it acts the same as the corresponding one without it.
template <typename ReturnType,
          typename... ParamType,
          bool kNoexcept,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
class GuestParamsAndReturn<ReturnType (*)(ParamType..., ...) noexcept(kNoexcept),
                           kCallingConventionsVariant>
    : public GuestParamsAndReturn<ReturnType(ParamType..., ...) noexcept(kNoexcept),
                                  GuestAbi::kDefaultAbi> {
 public:
  GuestParamsAndReturn(ThreadState* state)
      : GuestParamsAndReturn<ReturnType(ParamType..., ...) noexcept(kNoexcept),
                             GuestAbi::kDefaultAbi>(state) {}
};

template <typename FunctionType,
          GuestAbi::CallingConventionsVariant kCallingConventionsVariant = GuestAbi::kDefaultAbi>
class GuestParamsValues;

class GuestVAListParams : GuestParamsAndReturnHelper {
 public:
  template <typename Func, GuestAbi::CallingConventionsVariant kCallingConventionsVariant>
  GuestVAListParams(const GuestParamsValues<Func, kCallingConventionsVariant>&& named_parameters)
      : calling_conventions_(named_parameters.kVAStartBase,
                             bit_cast<uint32_t>(named_parameters.s_)),
        r_(named_parameters.r_) {}

  // Initialize with va_list which is pointer to parameters which are located
  // as if they are passed on the stack.
  GuestVAListParams(GuestAddr va)
      : calling_conventions_(arm::CallingConventions::kStackOnly, va), r_(nullptr) {}

  template <typename T>
  typename GuestArgumentInfo<T, GuestAbi::kDefaultAbi>::GuestType& GetParam() {
    static_assert(GuestArgumentInfo<T, GuestAbi::kDefaultAbi>::kArgumentClass ==
                  ArgumentClass::kInteger);
    return *this->ParamLocationAddress<T, GuestAbi::kDefaultAbi>(
        r_,
        nullptr,
        nullptr,
        calling_conventions_.GetNextIntArgLoc(
            GuestArgumentInfo<T, GuestAbi::kDefaultAbi>::kSize,
            GuestArgumentInfo<T, GuestAbi::kDefaultAbi>::kAlignment));
  }

  template <typename T>
  T* GetPointerParam() {
    return ToHostAddr<T>(GetParam<GuestAddr>());
  }

 protected:
  static GuestAddr get_sp(ThreadState* state) { return state->cpu.r[13]; }

  arm::CallingConventions calling_conventions_;
  uint32_t* r_;
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_ABI_GUEST_PARAMS_ARCH_H_
