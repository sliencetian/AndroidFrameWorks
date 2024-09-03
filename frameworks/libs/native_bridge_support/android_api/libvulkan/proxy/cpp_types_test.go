//
// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

package cpp_types

import (
	"testing"
)

func TestStruct(t *testing.T) {
	assertLayout := func(t *testing.T, typе Type, arch Arch, arch_name string, align, size uint) {
		if typе.Align(arch) != align {
			t.Error("Wrong alignment on " + arch_name)
		}
		if typе.Bits(arch) != size {
			t.Error("Wrong size on " + arch_name)
		}
		if typе.Field(1, arch).Name() != "field2" {
			t.Error("Wrong fieldname on " + arch_name)
		}
		if typе.Field(1, arch).(StructFieldInfo).Offset() != align {
			t.Error("Wrong offset on " + arch_name)
		}
	}

	typе := StructType("TestStruct", []FieldInfo{
		Field("field1", Int8TType),
		Field("field2", Int8TType),
	})
	assertLayout(t, typе, Arm, "ARM", 8, 16)
	assertLayout(t, typе, Arm64, "ARM64", 8, 16)
	assertLayout(t, typе, Riscv32, "RISCV32", 8, 16)
	assertLayout(t, typе, Riscv64, "RISCV64", 8, 16)
	assertLayout(t, typе, X86, "X86", 8, 16)
	assertLayout(t, typе, X86_64, "X86-64", 8, 16)

	typе = StructType("TestStruct", []FieldInfo{
		Field("field1", Int32TType),
		Field("field2", Int64TType),
	})
	assertLayout(t, typе, Arm, "ARM", 64, 128)
	assertLayout(t, typе, Arm64, "ARM64", 64, 128)
	assertLayout(t, typе, Riscv32, "RISCV32", 64, 128)
	assertLayout(t, typе, Riscv64, "RISCV64", 64, 128)
	assertLayout(t, typе, X86, "X86", 32, 96)
	assertLayout(t, typе, X86_64, "X86-64", 64, 128)
}

type extendedField struct {
	FieldInfo
	extra_info string
}

type extendedFieldInfo interface {
	extraInfo() string
}

func (field_info *extendedField) BaseFieldInfo() FieldInfo {
	return field_info
}

func (field_info *extendedField) extraInfo() string {
	return field_info.extra_info
}

func TestExtendedField(t *testing.T) {
	assertLayout := func(t *testing.T, typе Type, arch Arch, arch_name string, align, size uint) {
		if typе.Align(arch) != align {
			t.Error("Wrong alignment on " + arch_name)
		}
		if typе.Bits(arch) != size {
			t.Error("Wrong size on " + arch_name)
		}
		field1 := typе.Field(0, arch).BaseFieldInfo().(extendedFieldInfo)
		if field1.extraInfo() != "This is extra info for field1" {
			t.Error("Wrong extra info on " + arch_name)
		}
		field2 := typе.Field(1, arch).BaseFieldInfo().(extendedFieldInfo)
		if field2.extraInfo() != "This is extra info for field2" {
			t.Error("Wrong extra info on " + arch_name)
		}
	}

	typе := StructType("TestStruct", []FieldInfo{
		&extendedField{Field("field1", Int8TType), "This is extra info for field1"},
		&extendedField{Field("field2", Int8TType), "This is extra info for field2"},
	})
	assertLayout(t, typе, Arm, "ARM", 8, 16)
	assertLayout(t, typе, Arm64, "ARM64", 8, 16)
	assertLayout(t, typе, Riscv32, "RISCV32", 8, 16)
	assertLayout(t, typе, Riscv64, "RISCV64", 8, 16)
	assertLayout(t, typе, X86, "X86", 8, 16)
	assertLayout(t, typе, X86_64, "X86-64", 8, 16)

	typе = UnionType("TestStruct", []FieldInfo{
		&extendedField{Field("field1", Int8TType), "This is extra info for field1"},
		&extendedField{Field("field2", Int8TType), "This is extra info for field2"},
	})
	assertLayout(t, typе, Arm, "ARM", 8, 8)
	assertLayout(t, typе, Arm64, "ARM64", 8, 8)
	assertLayout(t, typе, Riscv32, "RISCV32", 8, 8)
	assertLayout(t, typе, Riscv64, "RISCV64", 8, 8)
	assertLayout(t, typе, X86, "X86", 8, 8)
	assertLayout(t, typе, X86_64, "X86-64", 8, 8)
}

func TestNameOfConstWithPointer(t *testing.T) {
	const_pointer := ConstType(PointerType(IntType))
	if const_pointer.Name(FirstArch) != "int *const" {
		t.Error("Wrong name of pointer to array")
	}
	const_pointer = PointerType(ConstType(IntType))
	if const_pointer.Name(FirstArch) != "const int *" {
		t.Error("Wrong name of pointer to array")
	}
	const_pointer = ConstType(PointerType(ConstType(IntType)))
	if const_pointer.Name(FirstArch) != "const int *const" {
		t.Error("Wrong name of pointer to array")
	}
}

func TestNameOfConstWithArrayAndPointer(t *testing.T) {
	const_pointer := ConstType(PointerType(ArrayType(IntType, 10)))
	if const_pointer.Name(FirstArch) != "int (*const)[10]" {
		t.Error("Wrong name of pointer to array")
	}
	const_pointer = PointerType(ArrayType(ConstType(IntType), 10))
	if const_pointer.Name(FirstArch) != "const int (*)[10]" {
		t.Error("Wrong name of pointer to array")
	}
	const_pointer = ConstType(PointerType(ArrayType(ConstType(IntType), 10)))
	if const_pointer.Name(FirstArch) != "const int (*const)[10]" {
		t.Error("Wrong name of pointer to array")
	}
}

func TestSignalFuncTypeName(t *testing.T) {
	signal := PointerType(FunctionType(
		PointerType(FunctionType(VoidType, []FieldInfo{Field("sig1", IntType)})),
		[]FieldInfo{
			Field("sig2", IntType),
			Field("handler", PointerType(FunctionType(
				VoidType, []FieldInfo{Field("sig3", IntType)})))}))
	if signal.Name(FirstArch) != "void (*(*)(int sig2, void (*handler)(int sig3)))(int sig1)" {
		t.Error("Wrong name of signal function")
	}
}

func TestIsCompatible(t *testing.T) {
	if !IsInputCompatible(UInt64TType, X86, Arm) {
		t.Error("uint64_t should be compatible in arm=>x86 direction")
	}
	if IsInputCompatible(UInt64TType, Arm, X86) {
		t.Error("uint64_t should be incompatible in x86=>arm direction")
	}
	if IsInputCompatible(StructType("Test", []FieldInfo{
		Field("a", UInt64TType),
		Field("b", UInt32TType)}),
		X86, Arm) {
		t.Error("Structure with uint64_t and uint32_t fields should be incompatible between arm and x86")
	}
	if IsInputCompatible(StructType("Test", []FieldInfo{
		Field("a", UInt32TType),
		Field("b", UInt64TType)}),
		X86, Arm) {
		t.Error("Structure with uint64_t and uint32_t fields should be incompatible between arm and x86")
	}
	if IsInputCompatible(PointerType(FunctionType(IntType, []FieldInfo{})), X86, Arm) {
		t.Error("Pointers to functions are never compatible")
	}
	if IsInputCompatible(ArchDependentType(
		EnumType("open_flags", IntType, []EnumFieldInfo{
			EnumField("open", IntType, "", 1),
			EnumField("close", IntType, "", 2)}),
		EnumType("open_flags", IntType, []EnumFieldInfo{
			EnumField("open", IntType, "", 1),
			EnumField("close", IntType, "", 2)}),
		EnumType("open_flags", IntType, []EnumFieldInfo{
			EnumField("open", IntType, "", 1),
			EnumField("close", IntType, "", 2)}),
		EnumType("open_flags", IntType, []EnumFieldInfo{
			EnumField("open", IntType, "", 1),
			EnumField("close", IntType, "", 2)}),
		EnumType("open_flags", IntType, []EnumFieldInfo{
			EnumField("open", IntType, "", 2),
			EnumField("close", IntType, "", 1)}),
		EnumType("open_flags", IntType, []EnumFieldInfo{
			EnumField("open", IntType, "", 1),
			EnumField("close", IntType, "", 2)})), Arm, X86) {
		t.Error("Enums with different values should be incompatible between arm and x86")
	}
}
