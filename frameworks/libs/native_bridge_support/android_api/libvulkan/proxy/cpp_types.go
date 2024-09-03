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
	"fmt"
	"strings"
)

type Type interface {
	Align(arch Arch) uint
	Bits(arch Arch) uint
	DeclareVar(var_name string, arch Arch) string
	BaseName(arch Arch) string // Name without additional marks: “*”, “[]”, “struct”, “union”, etc. Not defined for function pointers.
	Name(arch Arch) string
	// Note: some types are defined differently depending on architecture.
	// E.g. VK_DEFINE_NON_DISPATCHABLE_HANDLE is pointer on 64-bit platforms and uint64_t on 32-bit ones.
	Kind(arch Arch) Kind
	// Only for integer types
	Signed(arch Arch) bool
	// Only for Array or Ptr..
	Elem(arch Arch) Type
	// Only for Struct and Union
	NumField(arch Arch) uint
	Field(i uint, arch Arch) FieldInfo
}

// This is builder-only interface. Should only be used when you need to build recursive data types.
type ModifyablePtrType interface {
	ReplaceElem(pointee_type Type)
}

// FieldInfo interface can be expanded: StructFieldInfo is used for structs, EnumFieldInfo for enums.
//
// But since Go doesn't yet have generics StructFieldInfo carries information calculated in StructType
// constructor and also includes reference to the builder-provided type which may carry additional data.
// This creates “love triangle” of sorts:
//
//    in cpp_types                     in builder           in cpp_types
//                                 ┌┄┄┄┐
//                                 ┆   ▼
//    StructFieldInfo ─────────────┼─▶ BaseInfo ──────────▶ FieldInfo
//      Name() ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┼┄┄┄▶ Name() ┄┄┄┄┄┄┄┄┄┄┄┄▶ Name()
//      Type() ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┼┄┄┄▶ Type() ┄┄┄┄┄┄┄┄┄┄┄┄▶ Type()
//                       (returns) ┆     SomeOtherInfo()
//      BaseFieldInfo() ┄┄┄┄┄┄┄┄┄┄┄┘
//      Offset() (calculated during StructFieldInfo construction)
//
//
// This leaves SomeOtherInfo() provided by builder inaccessible directly.
// To access it one need to call BaseFieldInfo().
//
// But this means that we would need to distinguish cases where we have StructFieldInfo()
// (used for struct types) and other types of FieldInfo (used for unions, enums, functions).
//
// To make access consistent BaseFieldInfo() function is provided in FieldInfo, EnumFieldInfo (and
// is supposed to be provided by extended types).  It just returns pointer to xxxFieldInfo itself
// (StructFieldInfo is the only exception).
//
// Note: all structs fieldInfo, enumFieldInfo and structFieldInfo are constructed by the builder
// explicitly. Every fieldInfo may get SomeOtherInfo() attached. structFieldInfo may reference
// SomeOtherInfo through its base type. enumFieldInfo typically doesn't have and doesn't need
// SomeOtherInfo but can attach it like fieldInfo. To access SomeOtherInfo uniformly for fieldInfo,
// enumFieldInfo, and structFieldInfo we add BaseFieldInfo() method, which will return itself for
// fieldInfo and enumFieldInfo, and return base type for structFieldInfo.

// This way calling field_info.BaseFieldInfo().(BaseInfo).SomeOtherInfo() will give us access to
// SomeOtherInfo. Note: calling it for type which doesn't have it will panic.

type FieldInfo interface {
	Name() string
	Type() Type
	BaseFieldInfo() FieldInfo
}

type StructFieldInfo interface {
	Name() string
	Type() Type
	BaseFieldInfo() FieldInfo
	Offset() uint
}

type EnumFieldInfo interface {
	Name() string
	Type() Type
	BaseFieldInfo() FieldInfo
	Alias() string
	Value() int64
}

// Arches - both host and guest.
type Arch uint

const (
	Arm Arch = iota
	Arm64
	Riscv32
	Riscv64
	X86
	X86_64
	FirstArch = Arm
	LastArch  = X86_64
)

func Define(arch Arch) string {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return "defined(__arm__)"
	case Arm64:
		return "defined(__aarch64__)"
	case Riscv32:
		return "defined(__riscv) && (__riscv_xlen == 32)"
	case Riscv64:
		return "defined(__riscv) && (__riscv_xlen == 64)"
	case X86:
		return "defined(__i386__)"
	case X86_64:
		return "defined(__x86_64__)"
	}
}

// The zero Kind is Invalid Kind.
type Kind uint

const (
	Invalid Kind = iota
	Opaque
	Alias
	Void
	Bool
	Char16T
	Char32T
	Char
	SChar
	UChar
	Short
	UShort
	Int
	UInt
	Long
	ULong
	LongLong
	ULongLong
	SSizeT
	SizeT
	IntPtrT
	UIntPtrT
	Int8T
	UInt8T
	Int16T
	UInt16T
	Int32T
	UInt32T
	Int64T
	UInt64T
	Float32
	Float64
	Array
	Struct
	Union
	Ptr
	Enum
	Func
	Const
)

func AliasType(name string, base_type Type) Type {
	return &aliasType{name, base_type}
}

func OpaqueType(name string) Type {
	return &opaqueType{Opaque, name}
}

func ArchDependentType(arm_type, arm64_type, riscv32_type, riscv64_type, x86_type, x86_64_type Type) Type {
	return &archDependentType{arm_type, arm64_type, riscv32_type, riscv64_type, x86_type, x86_64_type}
}

func ConstType(base_type Type) Type {
	return &constType{base_type}
}

func PointerType(pointee_type Type) Type {
	return &pointerType{pointee_type}
}

func FunctionType(result Type, field_info []FieldInfo) Type {
	return &functionType{result, field_info}
}

func ArrayType(elem_type Type, size uint) Type {
	return &arrayType{elem_type, size}
}

// Note that this function should have the following prototype:
//
//	func StructType[FieldInfo BaseInfo](name string, fields_info []BaseInfo) Type;
//
// This way we may extend BaseInfo (potentially defined in other package) into
// structFieldInfo[BaseInfo] — which would be a generic type, too.
// Then out fields would support both StructFieldInfo interface and BaseInfo interface and remove
// BaseFieldInfo() function and related trick.
func StructType(name string, fields_info []FieldInfo) Type {
	arch_dependent_layout := false
	var struct_fields_info [LastArch + 1][]StructFieldInfo
	var offset [LastArch + 1]uint
	var align [LastArch + 1]uint
	for arch := FirstArch; arch <= LastArch; arch++ {
		struct_fields_info[arch] = make([]StructFieldInfo, len(fields_info))
		offset[arch] = 0
		align[arch] = 0
	}
	for i, field_info := range fields_info {
		for arch := FirstArch; arch <= LastArch; arch++ {
			field_align := field_info.Type().Align(arch)
			if align[arch] < field_align {
				align[arch] = field_align
			}
			modulo := offset[arch] % field_align
			if modulo != 0 {
				offset[arch] += field_align - modulo
			}
			struct_fields_info[arch][i] = &structFieldInfo{field_info, offset[arch]}
			offset[arch] += field_info.Type().Bits(arch)
			if align[FirstArch] != align[arch] || offset[FirstArch] != offset[arch] {
				arch_dependent_layout = true
			}
		}
	}
	for arch := FirstArch; arch <= LastArch; arch++ {
		modulo := offset[arch] % align[arch]
		if modulo != 0 {
			offset[arch] += align[arch] - modulo
		}
		if offset[FirstArch] != offset[arch] {
			arch_dependent_layout = true
		}
	}
	if arch_dependent_layout {
		return &archDependentType{
			&structType{name, struct_fields_info[Arm], align[Arm], offset[Arm]},
			&structType{name, struct_fields_info[Arm64], align[Arm64], offset[Arm64]},
			&structType{name, struct_fields_info[Riscv32], align[Riscv32], offset[Riscv32]},
			&structType{name, struct_fields_info[Riscv64], align[Riscv64], offset[Riscv64]},
			&structType{name, struct_fields_info[X86], align[X86], offset[X86]},
			&structType{name, struct_fields_info[X86_64], align[X86_64], offset[X86_64]}}
	} else {
		return &structType{name, struct_fields_info[FirstArch], align[FirstArch], offset[FirstArch]}
	}
}

func UnionType(name string, fields_info []FieldInfo) Type {
	arch_dependent_layout := false
	var bits [LastArch + 1]uint
	var align [LastArch + 1]uint
	for arch := FirstArch; arch <= LastArch; arch++ {
		bits[arch] = 0
		align[arch] = 0
		for _, field_info := range fields_info {
			typе := field_info.Type()
			if bits[arch] < typе.Bits(arch) {
				bits[arch] = typе.Bits(arch)
			}
			if align[arch] < typе.Align(arch) {
				align[arch] = typе.Align(arch)
			}
		}
		if align[FirstArch] != align[arch] || bits[FirstArch] != bits[arch] {
			arch_dependent_layout = true
		}
	}
	if arch_dependent_layout {
		return &archDependentType{
			&unionType{name, fields_info, align[Arm], bits[Arm]},
			&unionType{name, fields_info, align[Arm64], bits[Arm64]},
			&unionType{name, fields_info, align[Riscv32], bits[Riscv32]},
			&unionType{name, fields_info, align[Riscv64], bits[Riscv64]},
			&unionType{name, fields_info, align[X86], bits[X86]},
			&unionType{name, fields_info, align[X86_64], bits[X86_64]}}
	} else {
		return &unionType{name, fields_info, align[FirstArch], bits[FirstArch]}
	}
}

func Field(name string, typе Type) FieldInfo {
	return &fieldInfo{name, typе}
}

func EnumType(name string, basetype Type, values []EnumFieldInfo) Type {
	return &enumType{name, basetype, values}
}

func EnumField(name string, basetype Type, alias string, value int64) EnumFieldInfo {
	return &enumFieldInfo{name, basetype, alias, value}
}

var VoidType Type = &opaqueType{Void, "void"}

var BoolType Type = &fixedType{8, 8, Bool, "bool"}

var CharType Type = &archDependentType{
	&unsignedFixedType{fixedType{8, 8, Char, "char"}},
	&unsignedFixedType{fixedType{8, 8, Char, "char"}},
	&unsignedFixedType{fixedType{8, 8, Char, "char"}},
	&unsignedFixedType{fixedType{8, 8, Char, "char"}},
	&signedFixedType{fixedType{8, 8, Char, "char"}},
	&signedFixedType{fixedType{8, 8, Char, "char"}}}

var Char16TType Type = &unsignedFixedType{fixedType{16, 16, Char16T, "char16_t"}}

var Char32TType Type = &unsignedFixedType{fixedType{32, 32, Char32T, "char32_t"}}

var SCharType Type = &signedFixedType{fixedType{8, 8, SChar, "signed char"}}

var UCharType Type = &unsignedFixedType{fixedType{8, 8, UChar, "signed char"}}

var ShortType Type = &signedFixedType{fixedType{16, 16, Short, "short"}}

var UShortType Type = &unsignedFixedType{fixedType{16, 16, UShort, "unisigned short"}}

var IntType Type = &signedFixedType{fixedType{32, 32, Int, "int"}}

var UIntType Type = &unsignedFixedType{fixedType{32, 32, UInt, "unsigned int"}}

var LongType Type = &archDependentType{
	&signedFixedType{fixedType{32, 32, Long, "long"}},
	&signedFixedType{fixedType{64, 64, Long, "long"}},
	&signedFixedType{fixedType{32, 32, Long, "long"}},
	&signedFixedType{fixedType{64, 64, Long, "long"}},
	&signedFixedType{fixedType{32, 32, Long, "long"}},
	&signedFixedType{fixedType{64, 64, Long, "long"}}}

var ULongType Type = &archDependentType{
	&unsignedFixedType{fixedType{32, 32, ULong, "unsigned long"}},
	&unsignedFixedType{fixedType{64, 64, ULong, "unsigned long"}},
	&unsignedFixedType{fixedType{32, 32, ULong, "unsigned long"}},
	&unsignedFixedType{fixedType{64, 64, ULong, "unsigned long"}},
	&unsignedFixedType{fixedType{32, 32, ULong, "unsigned long"}},
	&unsignedFixedType{fixedType{64, 64, ULong, "unsigned long"}}}

var LongLongType Type = &archDependentType{
	&signedFixedType{fixedType{64, 64, LongLong, "long long"}},
	&signedFixedType{fixedType{64, 64, LongLong, "long long"}},
	&signedFixedType{fixedType{64, 64, LongLong, "long long"}},
	&signedFixedType{fixedType{64, 64, LongLong, "long long"}},
	&signedFixedType{fixedType{64, 32, LongLong, "long long"}},
	&signedFixedType{fixedType{64, 64, LongLong, "long long"}}}

var ULongLongType Type = &archDependentType{
	&unsignedFixedType{fixedType{64, 64, ULongLong, "unsigned long long"}},
	&unsignedFixedType{fixedType{64, 64, ULongLong, "unsigned long long"}},
	&unsignedFixedType{fixedType{64, 64, ULongLong, "unsigned long long"}},
	&unsignedFixedType{fixedType{64, 64, ULongLong, "unsigned long long"}},
	&unsignedFixedType{fixedType{64, 32, ULongLong, "unsigned long long"}},
	&unsignedFixedType{fixedType{64, 64, ULongLong, "unsigned long long"}}}

// Note: ssize_t is POSIX, not ISO C/C++! That's why it's not std::ssize_t
var SSizeTType Type = &archDependentType{
	&signedFixedType{fixedType{32, 32, SSizeT, "ssize_t"}},
	&signedFixedType{fixedType{64, 64, SSizeT, "ssize_t"}},
	&signedFixedType{fixedType{32, 32, SSizeT, "ssize_t"}},
	&signedFixedType{fixedType{64, 64, SSizeT, "ssize_t"}},
	&signedFixedType{fixedType{32, 32, SSizeT, "ssize_t"}},
	&signedFixedType{fixedType{64, 64, SSizeT, "ssize_t"}}}

var SizeTType Type = &archDependentType{
	&unsignedFixedType{fixedType{32, 32, SizeT, "std::size_t"}},
	&unsignedFixedType{fixedType{64, 64, SizeT, "std::size_t"}},
	&unsignedFixedType{fixedType{32, 32, SizeT, "std::size_t"}},
	&unsignedFixedType{fixedType{64, 64, SizeT, "std::size_t"}},
	&unsignedFixedType{fixedType{32, 32, SizeT, "std::size_t"}},
	&unsignedFixedType{fixedType{64, 64, SizeT, "std::size_t"}}}

var IntPtrTType Type = &archDependentType{
	&signedFixedType{fixedType{32, 32, IntPtrT, "std::intptr_t"}},
	&signedFixedType{fixedType{64, 64, IntPtrT, "std::intptr_t"}},
	&signedFixedType{fixedType{32, 32, IntPtrT, "std::intptr_t"}},
	&signedFixedType{fixedType{64, 64, IntPtrT, "std::intptr_t"}},
	&signedFixedType{fixedType{32, 32, IntPtrT, "std::intptr_t"}},
	&signedFixedType{fixedType{64, 64, IntPtrT, "std::intptr_t"}}}

var UintPtrTType Type = &archDependentType{
	&unsignedFixedType{fixedType{32, 32, UIntPtrT, "std::uintptr_t"}},
	&unsignedFixedType{fixedType{64, 64, UIntPtrT, "std::uintptr_t"}},
	&unsignedFixedType{fixedType{32, 32, UIntPtrT, "std::uintptr_t"}},
	&unsignedFixedType{fixedType{64, 64, UIntPtrT, "std::uintptr_t"}},
	&unsignedFixedType{fixedType{32, 32, UIntPtrT, "std::uintptr_t"}},
	&unsignedFixedType{fixedType{64, 64, UIntPtrT, "std::uintptr_t"}}}

var Int8TType Type = &signedFixedType{fixedType{8, 8, Int8T, "std::int8_t"}}

var UInt8TType Type = &unsignedFixedType{fixedType{8, 8, UInt8T, "std::uint8_t"}}

var Int16TType Type = &signedFixedType{fixedType{16, 16, Int16T, "std::int16_t"}}

var UInt16TType Type = &unsignedFixedType{fixedType{16, 16, UInt16T, "std::uint16_t"}}

var Int32TType Type = &signedFixedType{fixedType{32, 32, Int32T, "std::int32_t"}}

var UInt32TType Type = &unsignedFixedType{fixedType{32, 32, UInt32T, "std::uint32_t"}}

var Int64TType Type = &archDependentType{
	&signedFixedType{fixedType{64, 64, Int64T, "std::int64_t"}},
	&signedFixedType{fixedType{64, 64, Int64T, "std::int64_t"}},
	&signedFixedType{fixedType{64, 64, Int64T, "std::int64_t"}},
	&signedFixedType{fixedType{64, 64, Int64T, "std::int64_t"}},
	&signedFixedType{fixedType{64, 32, Int64T, "std::int64_t"}},
	&signedFixedType{fixedType{64, 64, Int64T, "std::int64_t"}}}

var UInt64TType Type = &archDependentType{
	&unsignedFixedType{fixedType{64, 64, UInt64T, "std::uint64_t"}},
	&unsignedFixedType{fixedType{64, 64, UInt64T, "std::uint64_t"}},
	&unsignedFixedType{fixedType{64, 64, UInt64T, "std::uint64_t"}},
	&unsignedFixedType{fixedType{64, 64, UInt64T, "std::uint64_t"}},
	&unsignedFixedType{fixedType{64, 32, UInt64T, "std::uint64_t"}},
	&unsignedFixedType{fixedType{64, 64, UInt64T, "std::uint64_t"}}}

var Float32Type Type = &signedFixedType{fixedType{32, 32, Float32, "float"}}

var Float64Type Type = &archDependentType{
	&signedFixedType{fixedType{64, 64, Float64, "double"}},
	&signedFixedType{fixedType{64, 64, Float64, "double"}},
	&signedFixedType{fixedType{64, 64, Float64, "double"}},
	&signedFixedType{fixedType{64, 64, Float64, "double"}},
	&signedFixedType{fixedType{64, 32, Float64, "double"}},
	&signedFixedType{fixedType{64, 64, Float64, "double"}}}

type opaqueType struct {
	kind Kind
	name string
}

func (typе *opaqueType) Align(Arch) uint {
	panic("cpp_types: Attempt to find out alignment of opaque type " + typе.name)
}

func (typе *opaqueType) Bits(Arch) uint {
	panic("cpp_types: Attempt to find out size of opaque type " + typе.name)
}

func (typе *opaqueType) DeclareVar(var_name string, arch Arch) string {
	panic("cpp_types: Attempt to create variable of opaque type " + typе.name)
}

func (typе *opaqueType) BaseName(Arch) string {
	return typе.name
}

func (typе *opaqueType) Name(Arch) string {
	return typе.name
}

func (typе *opaqueType) Kind(Arch) Kind {
	return typе.kind
}

func (typе *opaqueType) Elem(Arch) Type {
	panic("cpp_types: Calling Elem() for non-array type " + typе.name)
}

func (typе *opaqueType) Field(uint, Arch) FieldInfo {
	panic("cpp_types: Calling Field() for non-struct type " + typе.name)
}

func (typе *opaqueType) NumField(Arch) uint {
	panic("cpp_types: Calling NumField() for non-struct type " + typе.name)
}

func (typе *opaqueType) Signed(Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.name)
}

type aliasType struct {
	name      string
	base_type Type
}

func (typе *aliasType) Align(arch Arch) uint {
	return typе.base_type.Align(arch)
}

func (typе *aliasType) Bits(arch Arch) uint {
	return typе.base_type.Bits(arch)
}

func (typе *aliasType) DeclareVar(var_name string, arch Arch) string {
	return typе.name + " " + var_name
}

func (typе *aliasType) BaseName(Arch) string {
	return typе.name
}

func (typе *aliasType) Name(Arch) string {
	return typе.name
}

func (*aliasType) Kind(Arch) Kind {
	return Alias
}

func (typе *aliasType) Elem(arch Arch) Type {
	return typе.base_type
}

func (typе *aliasType) Field(uint, Arch) FieldInfo {
	panic("cpp_types: Calling Field() for non-struct type " + typе.name)
}

func (typе *aliasType) NumField(Arch) uint {
	panic("cpp_types: Calling NumField() for non-struct type " + typе.name)
}

func (typе *aliasType) Signed(Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.name)
}

type fixedType struct {
	size  uint
	align uint
	kind  Kind
	name  string
}

type signedFixedType struct {
	fixedType
}

type unsignedFixedType struct {
	fixedType
}

func (typе *fixedType) Align(Arch) uint {
	return typе.align
}

func (typе *fixedType) Bits(Arch) uint {
	return typе.size
}

func (typе *fixedType) DeclareVar(var_name string, arch Arch) string {
	return typе.name + " " + var_name
}

func (typе *fixedType) BaseName(Arch) string {
	return typе.name
}

func (typе *fixedType) Name(Arch) string {
	return typе.name
}

func (typе *fixedType) Kind(Arch) Kind {
	return typе.kind
}

func (typе *fixedType) Elem(Arch) Type {
	panic("cpp_types: Calling Elem() for non-array type " + typе.name)
}

func (typе *fixedType) Field(uint, Arch) FieldInfo {
	panic("cpp_types: Calling Field() for non-struct type " + typе.name)
}

func (typе *fixedType) NumField(Arch) uint {
	panic("cpp_types: Calling NumField() for non-struct type " + typе.name)
}

func (typе *fixedType) Signed(Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.name)
}

func (typе *signedFixedType) Signed(Arch) bool {
	return true
}

func (typе *unsignedFixedType) Signed(Arch) bool {
	return false
}

type archDependentType struct {
	arm_type     Type
	arm64_type   Type
	riscv32_type Type
	riscv64_type Type
	x86_type     Type
	x86_64_type  Type
}

func (typе *archDependentType) Align(arch Arch) uint {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Align(arch)
	case Arm64:
		return typе.arm64_type.Align(arch)
	case Riscv32:
		return typе.riscv32_type.Align(arch)
	case Riscv64:
		return typе.riscv64_type.Align(arch)
	case X86:
		return typе.x86_type.Align(arch)
	case X86_64:
		return typе.x86_64_type.Align(arch)
	}
}

func (typе *archDependentType) Bits(arch Arch) uint {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Bits(arch)
	case Arm64:
		return typе.arm64_type.Bits(arch)
	case Riscv32:
		return typе.riscv32_type.Bits(arch)
	case Riscv64:
		return typе.riscv64_type.Bits(arch)
	case X86:
		return typе.x86_type.Bits(arch)
	case X86_64:
		return typе.x86_64_type.Bits(arch)
	}
}

func (typе *archDependentType) DeclareVar(var_name string, arch Arch) string {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.DeclareVar(var_name, arch)
	case Arm64:
		return typе.arm64_type.DeclareVar(var_name, arch)
	case Riscv32:
		return typе.riscv32_type.DeclareVar(var_name, arch)
	case Riscv64:
		return typе.riscv64_type.DeclareVar(var_name, arch)
	case X86:
		return typе.x86_type.DeclareVar(var_name, arch)
	case X86_64:
		return typе.x86_64_type.DeclareVar(var_name, arch)
	}
}

func (typе *archDependentType) BaseName(arch Arch) string {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.BaseName(arch)
	case Arm64:
		return typе.arm64_type.BaseName(arch)
	case Riscv32:
		return typе.riscv32_type.BaseName(arch)
	case Riscv64:
		return typе.riscv64_type.BaseName(arch)
	case X86:
		return typе.x86_type.BaseName(arch)
	case X86_64:
		return typе.x86_64_type.BaseName(arch)
	}
}

func (typе *archDependentType) Name(arch Arch) string {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Name(arch)
	case Arm64:
		return typе.arm64_type.Name(arch)
	case Riscv32:
		return typе.riscv32_type.Name(arch)
	case Riscv64:
		return typе.riscv64_type.Name(arch)
	case X86:
		return typе.x86_type.Name(arch)
	case X86_64:
		return typе.x86_64_type.Name(arch)
	}
}

func (typе *archDependentType) Kind(arch Arch) Kind {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Kind(arch)
	case Arm64:
		return typе.arm64_type.Kind(arch)
	case Riscv32:
		return typе.riscv32_type.Kind(arch)
	case Riscv64:
		return typе.riscv64_type.Kind(arch)
	case X86:
		return typе.x86_type.Kind(arch)
	case X86_64:
		return typе.x86_64_type.Kind(arch)
	}
}

func (typе *archDependentType) Elem(arch Arch) Type {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Elem(arch)
	case Arm64:
		return typе.arm64_type.Elem(arch)
	case Riscv32:
		return typе.riscv32_type.Elem(arch)
	case Riscv64:
		return typе.riscv64_type.Elem(arch)
	case X86:
		return typе.x86_type.Elem(arch)
	case X86_64:
		return typе.x86_64_type.Elem(arch)
	}
}

func (typе *archDependentType) Field(i uint, arch Arch) FieldInfo {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Field(i, arch)
	case Arm64:
		return typе.arm64_type.Field(i, arch)
	case Riscv32:
		return typе.riscv32_type.Field(i, arch)
	case Riscv64:
		return typе.riscv64_type.Field(i, arch)
	case X86:
		return typе.x86_type.Field(i, arch)
	case X86_64:
		return typе.x86_64_type.Field(i, arch)
	}
}

func (typе *archDependentType) NumField(arch Arch) uint {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.NumField(arch)
	case Arm64:
		return typе.arm64_type.NumField(arch)
	case Riscv32:
		return typе.riscv32_type.NumField(arch)
	case Riscv64:
		return typе.riscv64_type.NumField(arch)
	case X86:
		return typе.x86_type.NumField(arch)
	case X86_64:
		return typе.x86_64_type.NumField(arch)
	}
}

func (typе *archDependentType) Signed(arch Arch) bool {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return typе.arm_type.Signed(arch)
	case Arm64:
		return typе.arm64_type.Signed(arch)
	case Riscv32:
		return typе.riscv32_type.Signed(arch)
	case Riscv64:
		return typе.riscv64_type.Signed(arch)
	case X86:
		return typе.x86_type.Signed(arch)
	case X86_64:
		return typе.x86_64_type.Signed(arch)
	}
}

type constType struct {
	base Type
}

func (typе *constType) Align(arch Arch) uint {
	return typе.base.Align(arch)
}

func (typе *constType) Bits(arch Arch) uint {
	return typе.base.Bits(arch)
}

func (typе *constType) DeclareVar(var_name string, arch Arch) string {
	if typе.base.Kind(arch) == Ptr {
		if len(var_name) >= 1 && (var_name[0] == '(' || var_name[0] == '[') {
			return typе.base.DeclareVar("const"+var_name, arch)
		} else {
			return typе.base.DeclareVar("const "+var_name, arch)
		}
	}
	return "const " + typе.base.DeclareVar(var_name, arch)
}

func (typе *constType) BaseName(arch Arch) string {
	return typе.base.BaseName(arch)
}

func (typе *constType) Name(arch Arch) string {
	if typе.base.Kind(arch) == Ptr {
		return typе.base.DeclareVar("const", arch)
	}
	return "const " + typе.base.Name(arch)
}

func (*constType) Kind(Arch) Kind {
	return Const
}

func (typе *constType) Elem(Arch) Type {
	return typе.base
}

func (typе *constType) Field(i uint, arch Arch) FieldInfo {
	panic("cpp_types: Calling Field() for non-struct type " + typе.Name(arch))
}

func (typе *constType) NumField(arch Arch) uint {
	panic("cpp_types: Calling NumField() for non-struct type " + typе.Name(arch))
}

func (typе *constType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

type pointerType struct {
	pointee Type
}

func (*pointerType) Align(arch Arch) uint {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return 32
	case Arm64:
		return 64
	case Riscv32:
		return 32
	case Riscv64:
		return 64
	case X86:
		return 32
	case X86_64:
		return 64
	}
}

func (*pointerType) Bits(arch Arch) uint {
	switch arch {
	default:
		panic(fmt.Sprintf("cpp_types: Unknown arch %d", arch))
	case Arm:
		return 32
	case Arm64:
		return 64
	case Riscv32:
		return 32
	case Riscv64:
		return 64
	case X86:
		return 32
	case X86_64:
		return 64
	}
}

func (typе *pointerType) DeclareVar(var_name string, arch Arch) string {
	switch typе.pointee.Kind(arch) {
	default:
		return typе.pointee.Name(arch) + " *" + var_name
	case Array, Func:
		return typе.pointee.DeclareVar("(*"+var_name+")", arch)
	}
}

func (typе *pointerType) BaseName(arch Arch) string {
	return typе.pointee.BaseName(arch)
}

func (typе *pointerType) Name(arch Arch) string {
	switch typе.pointee.Kind(arch) {
	default:
		return typе.pointee.Name(arch) + " *"
	case Array, Func:
		return typе.pointee.DeclareVar("(*)", arch)
	}
}

func (*pointerType) Kind(Arch) Kind {
	return Ptr
}

func (typе *pointerType) Elem(Arch) Type {
	return typе.pointee
}

func (typе *pointerType) Field(i uint, arch Arch) FieldInfo {
	panic("cpp_types: Calling Field() for non-struct type " + typе.Name(arch))
}

func (typе *pointerType) NumField(arch Arch) uint {
	panic("cpp_types: Calling NumField() for non-struct type " + typе.Name(arch))
}

func (typе *pointerType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

func (typе *pointerType) ReplaceElem(pointee_type Type) {
	for arch := FirstArch; arch <= LastArch; arch++ {
		real_pointee_type_kind := pointee_type.Kind(arch)
		if typе.pointee.Kind(arch) == Const && typе.pointee.Elem(arch).Kind(arch) == Opaque {
			if real_pointee_type_kind != Const {
				panic("cpp_types: Trying to replace const opaque type with non-const " + typе.Name(arch))
			}
			real_pointee_type_kind = pointee_type.Elem(arch).Kind(arch)
		} else if typе.pointee.Kind(arch) != Opaque {
			panic("cpp_types: Trying to replace non-opaque type " + typе.Name(arch))
		}
		if real_pointee_type_kind != Struct && real_pointee_type_kind != Union {
			panic("cpp_types: Trying to replace type with non-structural type " + typе.Name(arch))
		}
	}
	typе.pointee = pointee_type
}

type functionType struct {
	result Type
	params []FieldInfo
}

func (*functionType) Align(arch Arch) uint {
	panic("cpp_types: Calling Align for function type")
}

func (*functionType) Bits(arch Arch) uint {
	panic("cpp_types: Calling Align for function type")
}

func (typе *functionType) DeclareVar(var_name string, arch Arch) string {
	params := make([]string, len(typе.params))
	for i, param := range typе.params {
		params[i] = param.Type().DeclareVar(param.Name(), arch)
	}
	// Note: void is opaque type, it's forbidden to declare variable of type "void"
	if typе.result.Kind(arch) == Void {
		return "void " + var_name + "(" + strings.Join(params, ", ") + ")"
	} else {
		return typе.result.DeclareVar(var_name+"("+strings.Join(params, ", ")+")", arch)
	}
}

func (typе *functionType) BaseName(arch Arch) string {
	panic("cpp_types: Calling BaseName for function type")
}

func (typе *functionType) Name(arch Arch) string {
	return typе.DeclareVar("", arch)
}

func (*functionType) Kind(Arch) Kind {
	return Func
}

func (typе *functionType) Elem(Arch) Type {
	return typе.result
}

func (typе *functionType) Field(i uint, arch Arch) FieldInfo {
	return typе.params[i]
}

func (typе *functionType) NumField(arch Arch) uint {
	return uint(len(typе.params))
}

func (typе *functionType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

type arrayType struct {
	elem Type
	size uint
}

func (typе *arrayType) Align(arch Arch) uint {
	return typе.elem.Align(arch)
}

func (typе *arrayType) Bits(arch Arch) uint {
	return typе.elem.Bits(arch) * typе.size
}

func (typе *arrayType) DeclareVar(var_name string, arch Arch) string {
	return fmt.Sprintf("%s[%d]", typе.elem.DeclareVar(var_name, arch), typе.size)
}

func (typе *arrayType) BaseName(arch Arch) string {
	return typе.elem.Name(arch)
}

func (typе *arrayType) Name(arch Arch) string {
	return fmt.Sprintf("%s[%d]", typе.elem.Name(arch), typе.size)
}

func (*arrayType) Kind(Arch) Kind {
	return Array
}

func (typе *arrayType) Elem(Arch) Type {
	return typе.elem
}

func (typе *arrayType) Field(i uint, arch Arch) FieldInfo {
	panic("cpp_types: Calling Field() for non-struct type " + typе.Name(arch))
}

func (typе *arrayType) NumField(arch Arch) uint {
	return typе.size
}

func (typе *arrayType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

type structType struct {
	name   string
	fields []StructFieldInfo
	align  uint
	bits   uint
}

func (typе *structType) Align(arch Arch) uint {
	return typе.align
}

func (typе *structType) Bits(arch Arch) uint {
	return typе.bits
}

func (typе *structType) DeclareVar(var_name string, arch Arch) string {
	return "struct " + typе.name + " " + var_name
}

func (typе *structType) BaseName(arch Arch) string {
	return typе.name
}

func (typе *structType) Name(arch Arch) string {
	return "struct " + typе.name
}

func (*structType) Kind(Arch) Kind {
	return Struct
}

func (typе *structType) Elem(Arch) Type {
	panic("cpp_types: Calling Elem() for non-array type " + typе.name)
}

func (typе *structType) Field(i uint, arch Arch) FieldInfo {
	return typе.fields[i]
}

func (typе *structType) NumField(arch Arch) uint {
	return uint(len(typе.fields))
}

func (typе *structType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

type fieldInfo struct {
	name string
	typе Type
}

func (field_info *fieldInfo) Name() string {
	return field_info.name
}

func (field_info *fieldInfo) Type() Type {
	return field_info.typе
}

func (field_info *fieldInfo) BaseFieldInfo() FieldInfo {
	return field_info
}

type structFieldInfo struct {
	base_field_info FieldInfo
	offset          uint
}

func (field_info *structFieldInfo) Name() string {
	return field_info.base_field_info.Name()
}

func (field_info *structFieldInfo) Type() Type {
	return field_info.base_field_info.Type()
}

func (field_info *structFieldInfo) BaseFieldInfo() FieldInfo {
	return field_info.base_field_info
}

func (field_info *structFieldInfo) Offset() uint {
	return field_info.offset
}

type unionType struct {
	name   string
	fields []FieldInfo
	align  uint
	bits   uint
}

func (typе *unionType) Align(arch Arch) uint {
	return typе.align
}

func (typе *unionType) Bits(arch Arch) uint {
	return typе.bits
}

func (typе *unionType) DeclareVar(var_name string, arch Arch) string {
	return "union " + typе.name + " " + var_name
}

func (typе *unionType) BaseName(arch Arch) string {
	return typе.name
}

func (typе *unionType) Name(arch Arch) string {
	return "union " + typе.name
}

func (*unionType) Kind(Arch) Kind {
	return Union
}

func (typе *unionType) Elem(Arch) Type {
	panic("cpp_types: Calling Elem() for non-array type " + typе.name)
}

func (typе *unionType) Field(i uint, arch Arch) FieldInfo {
	return typе.fields[i]
}

func (typе *unionType) NumField(arch Arch) uint {
	return uint(len(typе.fields))
}

func (typе *unionType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

type enumType struct {
	name     string
	basetype Type
	fields   []EnumFieldInfo
}

func (typе *enumType) Align(arch Arch) uint {
	return typе.basetype.Align(arch)
}

func (typе *enumType) Bits(arch Arch) uint {
	return typе.basetype.Bits(arch)
}

func (typе *enumType) DeclareVar(var_name string, arch Arch) string {
	return typе.name + " " + var_name
}

func (typе *enumType) BaseName(arch Arch) string {
	return typе.name
}

func (typе *enumType) Name(arch Arch) string {
	return typе.name
}

func (*enumType) Kind(Arch) Kind {
	return Enum
}

func (typе *enumType) Elem(Arch) Type {
	return typе.basetype
}

func (typе *enumType) Field(i uint, arch Arch) FieldInfo {
	return typе.fields[i]
}

func (typе *enumType) NumField(arch Arch) uint {
	return uint(len(typе.fields))
}

func (typе *enumType) Signed(arch Arch) bool {
	panic("cpp_types: Calling Signed() for non-numeric type " + typе.Name(arch))
}

type enumFieldInfo struct {
	name  string
	typе  Type
	alias string
	value int64
}

func (field_info *enumFieldInfo) Name() string {
	return field_info.name
}

func (field_info *enumFieldInfo) Type() Type {
	return field_info.typе
}

func (field_info *enumFieldInfo) BaseFieldInfo() FieldInfo {
	return field_info
}

func (field_info *enumFieldInfo) Alias() string {
	return field_info.alias
}

func (field_info *enumFieldInfo) Value() int64 {
	return field_info.value
}

func IsCompatible(typе Type, host_arch, guest_arch Arch) bool {
	return IsInputCompatible(typе, host_arch, guest_arch) && IsInputCompatible(typе, guest_arch, host_arch)
}

func IsInputCompatible(typе Type, host_arch, guest_arch Arch) bool {
	return isInputCompatible(typе, host_arch, typе, guest_arch, make(map[string]Type))
}

func isInputCompatible(host_type Type, host_arch Arch, guest_type Type, guest_arch Arch, processed_structures map[string]Type) bool {
	kind := host_type.Kind(host_arch)
	if kind == Alias {
		return isInputCompatible(host_type.Elem(host_arch), host_arch, guest_type, guest_arch, processed_structures)
	}
	kind2 := guest_type.Kind(guest_arch)
	if kind2 == Alias {
		return isInputCompatible(host_type, host_arch, guest_type.Elem(host_arch), guest_arch, processed_structures)
	}
	if kind != kind2 {
		return false
	}
	// Functions are never automatically compatible
	if kind == Func {
		return false
	}
	// Strip const from both types (handles types like "const Func", "const void").
	if kind == Const {
		return isInputCompatible(host_type.Elem(host_arch), host_arch, guest_type.Elem(guest_arch), guest_arch, processed_structures)
	}
	// Opaque types and Void are compatible even if sizes and alignment are unknown
	if kind == Void || (kind == Opaque && host_type.Name(host_arch) == guest_type.Name(guest_arch)) {
		return true
	}
	if host_type.Bits(host_arch) != guest_type.Bits(guest_arch) {
		return false
	}
	// Objects in the guest memory should have at least the same alignment as in host to be passed to host functions.
	// For objects created in host memory we assume that guest code will never check their alignment.
	if guest_type.Align(guest_arch) < host_type.Align(host_arch) {
		return false
	}
	switch kind {
	case Array:
		return host_type.NumField(host_arch) == host_type.NumField(guest_arch) &&
			isInputCompatible(host_type.Elem(host_arch), host_arch, guest_type.Elem(guest_arch), guest_arch, processed_structures)
	case Enum:
		if !isInputCompatible(host_type.Elem(host_arch), host_arch, guest_type.Elem(guest_arch), guest_arch, processed_structures) {
			return false
		}
		for i := uint(0); i < host_type.NumField(host_arch); i++ {
			host_field_enum := host_type.Field(i, host_arch).(EnumFieldInfo)
			guest_field_enum := guest_type.Field(i, guest_arch).(EnumFieldInfo)
			if host_field_enum.Value() != guest_field_enum.Value() {
				return false
			}
		}
	case Ptr:
		return isInputCompatible(host_type.Elem(host_arch), host_arch, guest_type.Elem(guest_arch), guest_arch, processed_structures)
	case Struct, Union:
		name := host_type.Name(host_arch)
		if name != guest_type.Name(guest_arch) {
			return false
		}
		if _, ok := processed_structures[name]; ok {
			return true
		}
		processed_structures[name] = host_type
		if host_type.NumField(host_arch) != guest_type.NumField(guest_arch) {
			return false
		}
		for i := uint(0); i < host_type.NumField(host_arch); i++ {
			host_field := host_type.Field(i, host_arch)
			guest_field := guest_type.Field(i, guest_arch)
			if kind == Struct {
				host_field_struct := host_field.(StructFieldInfo)
				guest_field_struct := guest_field.(StructFieldInfo)
				if host_field_struct.Offset() != guest_field_struct.Offset() {
					return false
				}
			}
			if !isInputCompatible(host_field.Type(), host_arch, guest_field.Type(), guest_arch, processed_structures) {
				return false
			}
		}
		break
	}
	return true
}

func IsKind(typе Type, kinds []Kind) bool {
	for i, kind := range kinds {
		for arch := FirstArch; arch < LastArch; arch++ {
			if typе.Kind(arch) != typе.Kind(LastArch) {
				panic("cpp_types: Calling IsKind() for arch-specific type " + typе.Name(arch) + "/" + typе.Name(LastArch))
			}
		}
		if typе.Kind(FirstArch) != kind {
			return false
		}
		if i+1 != len(kinds) {
			typе = typе.Elem(FirstArch)
		}
	}
	return true
}
