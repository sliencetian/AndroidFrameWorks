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

// Assembler to produce RV64 instructions (no ABI version). Somewhat influenced by V8 assembler.

#ifndef BERBERIS_ASSEMBLER_RV64_H_
#define BERBERIS_ASSEMBLER_RV64_H_

#include <type_traits>  // std::is_same

#include "berberis/assembler/riscv.h"

namespace berberis::rv64 {

class Assembler : public riscv::Assembler<Assembler> {
 public:
  using BaseAssembler = riscv::Assembler<Assembler>;
  using FinalAssembler = Assembler;

  explicit Assembler(MachineCode* code) : BaseAssembler(code) {}

  using ShiftImmediate = BaseAssembler::Shift64Immediate;

  // Don't use templates here to enable implicit conversions.
#define BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(IntType)                                \
  static constexpr std::optional<ShiftImmediate> MakeShiftImmediate(IntType value) { \
    return BaseAssembler::MakeShift64Immediate(value);                               \
  }
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(int8_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(uint8_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(int16_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(uint16_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(int32_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(uint32_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(int64_t)
  BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE(uint64_t)
#undef BERBERIS_DEFINE_MAKE_SHIFT_IMMEDIATE

  friend BaseAssembler;

// Instructions.
#include "berberis/assembler/gen_assembler_rv64-inl.h"  // NOLINT generated file!

 private:
  Assembler() = delete;
  Assembler(const Assembler&) = delete;
  Assembler(Assembler&&) = delete;
  void operator=(const Assembler&) = delete;
  void operator=(Assembler&&) = delete;
};

inline void Assembler::Ld(Register arg0, const Label& label) {
  jumps_.push_back(Jump{&label, pc(), false});
  // First issue auipc to load top 20 bits of difference between pc and target address
  EmitUTypeInstruction<uint32_t{0x0000'0017}>(arg0, UImmediate{0});
  // The low 12 bite of difference will be encoded in the Ld instruction
  EmitITypeInstruction<uint32_t{0x0000'3003}>(arg0, Operand<Register, IImmediate>{.base = arg0});
}

inline void Assembler::Lwu(Register arg0, const Label& label) {
  jumps_.push_back(Jump{&label, pc(), false});
  // First issue auipc to load top 20 bits of difference between pc and target address
  EmitUTypeInstruction<uint32_t{0x0000'0017}>(arg0, UImmediate{0});
  // The low 12 bite of difference will be encoded in the Lwu instruction
  EmitITypeInstruction<uint32_t{0x0000'6003}>(arg0, Operand<Register, IImmediate>{.base = arg0});
}

inline void Assembler::Sd(Register arg0, const Label& label, Register arg2) {
  jumps_.push_back(Jump{&label, pc(), false});
  // First issue auipc to load top 20 bits of difference between pc and target address
  EmitUTypeInstruction<uint32_t{0x0000'0017}>(arg2, UImmediate{0});
  // The low 12 bite of difference will be encoded in the Sd instruction
  EmitSTypeInstruction<uint32_t{0x0000'3023}>(arg0, Operand<Register, SImmediate>{.base = arg2});
}

}  // namespace berberis::rv64

#endif  // BERBERIS_ASSEMBLER_RV64_H_
