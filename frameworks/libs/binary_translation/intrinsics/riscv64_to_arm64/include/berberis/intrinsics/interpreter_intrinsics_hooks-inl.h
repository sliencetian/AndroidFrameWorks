/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file excenaupt in compliance with the License.
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

// TODO(b/346603097): This is a temporary file. It will be replaced by
// genertated file.

Register Adduw(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoAdd(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoAdd<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoAnd(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoAnd<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoMax(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoMax<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoMin(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoMin<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoOr(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoOr<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoSwap(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoSwap<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

template <typename Type0, bool kBool1, bool kBool2>
Register AmoXor(Register arg0, Register arg1) const {
  return IntegerToGPRReg(std::get<0>(intrinsics::AmoXor<Type0, kBool1, kBool2>(
      GPRRegToInteger<int64_t>(arg0), GPRRegToInteger<Type0>(arg1))));
}

Register Bclr(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Bclri(Register arg0, uint8_t arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Bext(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Bexti(Register arg0, uint8_t arg1) const {
  UNUSED(arg0);
  UNUSED(arg1);
  return {};
}

Register Binv(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Binvi(Register arg0, uint8_t arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Bset(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Bseti(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister CanonicalizeNan(FpRegister arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Clz(Register arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Cpop(Register arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Ctz(Register arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Div(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FAdd(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3) const {
  UNUSED(arg0, arg1, arg2, arg3);
  return {};
}

template <typename Type0>
Register FClass(FpRegister arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0, typename Type1>
FpRegister FCvtFloatToFloat(int8_t arg0, Register arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0, typename Type1>
Register FCvtFloatToInteger(int8_t arg0, Register arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0, typename Type1>
Register FCvtFloatToIntegerHostRounding(FpRegister arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0, typename Type1>
FpRegister FCvtIntegerToFloat(int8_t arg0, Register arg1, Register arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
FpRegister FDiv(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3) const {
  UNUSED(arg0, arg1, arg2, arg3);
  return {};
}

template <typename Type0>
FpRegister FMAdd(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3, FpRegister arg4)
    const {
  UNUSED(arg0, arg1, arg2, arg3, arg4);
  return {};
}

template <typename Type0>
FpRegister FMAddHostRounding(FpRegister arg0, FpRegister arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
FpRegister FMSub(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3, FpRegister arg4)
    const {
  UNUSED(arg0, arg1, arg2, arg3, arg4);
  return {};
}

template <typename Type0>
FpRegister FMSubHostRounding(FpRegister arg0, FpRegister arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
FpRegister FMax(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FMin(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FMul(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3) const {
  UNUSED(arg0, arg1, arg2, arg3);
  return {};
}

template <typename Type0>
FpRegister FMulHostRounding(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FNMAdd(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3, FpRegister arg4)
    const {
  UNUSED(arg0, arg1, arg2, arg3, arg4);
  return {};
}

template <typename Type0>
FpRegister FNMAddHostRounding(FpRegister arg0, FpRegister arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
FpRegister FNMSub(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3, FpRegister arg4)
    const {
  UNUSED(arg0, arg1, arg2, arg3, arg4);
  return {};
}

template <typename Type0>
FpRegister FNMSubHostRounding(FpRegister arg0, FpRegister arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
FpRegister FSgnj(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FSgnjn(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FSgnjx(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister FSqrt(int8_t arg0, Register arg1, FpRegister arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
FpRegister FSqrtHostRounding(FpRegister arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
FpRegister FSub(int8_t arg0, Register arg1, FpRegister arg2, FpRegister arg3) const {
  UNUSED(arg0, arg1, arg2, arg3);
  return {};
}

template <typename Type0>
Register Feq(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
Register Fle(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
Register Flt(FpRegister arg0, FpRegister arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0, typename Type1>
Register FmvFloatToInteger(FpRegister arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0, typename Type1>
FpRegister FmvIntegerToFloat(Register arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Max(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
Register Min(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
FpRegister NanBox(FpRegister arg0) const {
  UNUSED(arg0);
  return {};
}

Register Orcb(Register arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Rem(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Rev8(Register arg0) const {
  UNUSED(arg0);
  return {};
}

template <typename Type0>
Register Rol(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
Register Ror(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

template <typename Type0>
Register Sext(Register arg0) const {
  UNUSED(arg0);
  return {};
}

Register Sh1add(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Sh1adduw(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Sh2add(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Sh2adduw(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Sh3add(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Sh3adduw(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

Register Slliuw(Register arg0, uint8_t arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

std::tuple<Register, Register> Vsetivli(uint8_t arg0, uint16_t arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

std::tuple<Register, Register> Vsetvl(Register arg0, Register arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

std::tuple<Register, Register> Vsetvli(Register arg0, uint16_t arg1) const {
  UNUSED(arg0, arg1);
  return {};
}

std::tuple<Register, Register> Vsetvlimax(uint16_t arg0) const {
  UNUSED(arg0);
  return {};
}

std::tuple<Register, Register> Vsetvlmax(Register arg0) const {
  UNUSED(arg0);
  return {};
}

std::tuple<Register, Register> Vtestvl(Register arg0, Register arg1, Register arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

std::tuple<Register, Register> Vtestvli(Register arg0, Register arg1, uint16_t arg2) const {
  UNUSED(arg0, arg1, arg2);
  return {};
}

template <typename Type0>
Register Zext(Register arg0) const {
  UNUSED(arg0);
  return {};
}