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

#ifndef BERBERIS_INTRINSICS_COMMON_INTRINSICS_BINDINGS_H_
#define BERBERIS_INTRINSICS_COMMON_INTRINSICS_BINDINGS_H_

#include <cstdint>

#include "berberis/base/dependent_false.h"
#include "berberis/intrinsics/intrinsics_args.h"
#include "berberis/intrinsics/type_traits.h"

namespace berberis::intrinsics::bindings {

class FLAGS {
 public:
  static constexpr bool kIsImmediate = false;
  static constexpr bool kIsImplicitReg = true;
  static constexpr char kAsRegister = 0;
  template <typename MachineInsnArch>
  static constexpr auto kRegClass = MachineInsnArch::kFLAGS;
};

class Mem8 {
 public:
  using Type = uint8_t;
  static constexpr bool kIsImmediate = false;
  static constexpr char kAsRegister = 'm';
};

class Mem16 {
 public:
  using Type = uint16_t;
  static constexpr bool kIsImmediate = false;
  static constexpr char kAsRegister = 'm';
};

class Mem32 {
 public:
  using Type = uint32_t;
  static constexpr bool kIsImmediate = false;
  static constexpr char kAsRegister = 'm';
};

class Mem64 {
 public:
  using Type = uint64_t;
  static constexpr bool kIsImmediate = false;
  static constexpr char kAsRegister = 'm';
};

// Tag classes. They are never instantioned, only used as tags to pass information about
// bindings.
class Def;
class DefEarlyClobber;
class Use;
class UseDef;

template <typename Tag, typename MachineRegKind>
constexpr auto ToRegKind() {
  if constexpr (std::is_same_v<Tag, Def>) {
    return MachineRegKind::kDef;
  } else if constexpr (std::is_same_v<Tag, DefEarlyClobber>) {
    return MachineRegKind::kDefEarlyClobber;
  } else if constexpr (std::is_same_v<Tag, Use>) {
    return MachineRegKind::kUse;
  } else if constexpr (std::is_same_v<Tag, UseDef>) {
    return MachineRegKind::kUseDef;
  } else {
    static_assert(kDependentTypeFalse<Tag>);
  }
}

template <typename Tag, typename MachineRegKind>
inline constexpr auto kRegKind = ToRegKind<Tag, MachineRegKind>();

// Tag classes. They are never instantioned, only used as tags to pass information about
// bindings.
class NoCPUIDRestriction;  // All CPUs have at least “no CPUID restriction” mode.

// Tag classes. They are never instantioned, only used as tags to pass information about
// bindings.
class NoNansOperation;
class PreciseNanOperationsHandling;
class ImpreciseNanOperationsHandling;

template <auto kIntrinsicTemplateName,
          auto kMacroInstructionTemplateName,
          auto kMnemo,
          typename GetOpcode,
          typename CPUIDRestrictionTemplateValue,
          typename PreciseNanOperationsHandlingTemplateValue,
          bool kSideEffectsTemplateValue,
          typename... Types>
class AsmCallInfo;

template <auto kIntrinsicTemplateName,
          auto kMacroInstructionTemplateName,
          auto kMnemo,
          typename GetOpcode,
          typename CPUIDRestrictionTemplateValue,
          typename PreciseNanOperationsHandlingTemplateValue,
          bool kSideEffectsTemplateValue,
          typename... InputArgumentsTypes,
          typename... OutputArgumentsTypes,
          typename... BindingsTypes>
class AsmCallInfo<kIntrinsicTemplateName,
                  kMacroInstructionTemplateName,
                  kMnemo,
                  GetOpcode,
                  CPUIDRestrictionTemplateValue,
                  PreciseNanOperationsHandlingTemplateValue,
                  kSideEffectsTemplateValue,
                  std::tuple<InputArgumentsTypes...>,
                  std::tuple<OutputArgumentsTypes...>,
                  BindingsTypes...>
    final {
 public:
  static constexpr auto kIntrinsic = kIntrinsicTemplateName;
  static constexpr auto kMacroInstruction = kMacroInstructionTemplateName;
  // TODO(b/260725458): Use lambda template argument after C++20 becomes available.
  template <typename Opcode>
  static constexpr auto kOpcode = GetOpcode{}.template operator()<Opcode>();
  using CPUIDRestriction = CPUIDRestrictionTemplateValue;
  using PreciseNanOperationsHandling = PreciseNanOperationsHandlingTemplateValue;
  static constexpr bool kSideEffects = kSideEffectsTemplateValue;
  static constexpr const char* InputArgumentsTypeNames[] = {
      TypeTraits<InputArgumentsTypes>::kName...};
  static constexpr const char* OutputArgumentsTypeNames[] = {
      TypeTraits<OutputArgumentsTypes>::kName...};
  template <typename Callback, typename... Args>
  constexpr static void ProcessBindings(Callback&& callback, Args&&... args) {
    (callback(ArgTraits<BindingsTypes>(), std::forward<Args>(args)...), ...);
  }
  template <typename Callback, typename... Args>
  constexpr static auto MakeTuplefromBindings(Callback&& callback, Args&&... args) {
    return std::tuple_cat(callback(ArgTraits<BindingsTypes>(), std::forward<Args>(args)...)...);
  }
  using InputArguments = std::tuple<InputArgumentsTypes...>;
  using OutputArguments = std::tuple<OutputArgumentsTypes...>;
  using Bindings = std::tuple<BindingsTypes...>;
  using IntrinsicType = std::conditional_t<std::tuple_size_v<OutputArguments> == 0,
                                           void (*)(InputArgumentsTypes...),
                                           OutputArguments (*)(InputArgumentsTypes...)>;
  template <template <typename, auto, auto, typename...> typename MachineInsnType,
            template <typename...>
            typename ConstructorArgs,
            typename Opcode>
  using MachineInsn = MachineInsnType<AsmCallInfo,
                                      kMnemo,
                                      kOpcode<Opcode>,
                                      ConstructorArgs<BindingsTypes...>,
                                      BindingsTypes...>;
};

}  // namespace berberis::intrinsics::bindings

#endif  // BERBERIS_INTRINSICS_COMMON_INTRINSICS_BINDINGS_H_
