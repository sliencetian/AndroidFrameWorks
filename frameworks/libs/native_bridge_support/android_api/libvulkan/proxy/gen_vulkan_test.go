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

package main

import (
	"berberis/cpp_types"
	"berberis/vulkan_xml"
	"testing"
)

func TestAreBaseTypesDeclared(t *testing.T) {
	foo_type := vulkan_xml.ExtendedStruct(cpp_types.StructType("Foo", []cpp_types.FieldInfo{
		cpp_types.Field("i", cpp_types.UInt32TType)}), false, "")
	bar_type := vulkan_xml.ExtendedStruct(cpp_types.StructType("Bar", []cpp_types.FieldInfo{
		cpp_types.Field("i", cpp_types.UInt32TType),
		cpp_types.Field("j", cpp_types.UInt64TType)}), false, "")
	declared_types := make(map[string]cpp_types.Type)
	if areBaseTypesDeclared(foo_type, declared_types) {
		t.Error("Directly referred type is not declared, but areBaseTypesDeclared reports it as declared")
	}
	if areBaseTypesDeclared(bar_type, declared_types) {
		t.Error("Directly referred type Bar is not declared, but areBaseTypesDeclared reports it as declared")
	}
	if areBaseTypesDeclared(cpp_types.ArrayType(foo_type, 1), declared_types) {
		t.Error("Directly referred array of type Foo is not declared, but areBaseTypesDeclared reports it as declared")
	}
	if areBaseTypesDeclared(cpp_types.ArrayType(bar_type, 1), declared_types) {
		t.Error("Directly referred array o type Bar is not declared, but areBaseTypesDeclared reports it as declared")
	}
	if areBaseTypesDeclared(cpp_types.PointerType(foo_type), declared_types) {
		t.Error("Pointer for non-opaque struct is not declared, but areBaseTypesDeclared reports it as declared")
	}
	if !areBaseTypesDeclared(cpp_types.PointerType(cpp_types.OpaqueType("struct Baz")), declared_types) {
		t.Error("Pointer for opaque struct must always be considered declared, but areBaseTypesDeclared reports it as not declared")
	}
	declared_types["struct Foo"] = foo_type
	declared_types["struct Bar"] = bar_type
	if !areBaseTypesDeclared(foo_type, declared_types) {
		t.Error("Directly referred defined types are declared, but areBaseTypesDeclared reports it as not declared")
	}
	if !areBaseTypesDeclared(bar_type, declared_types) {
		t.Error("Directly referred defined types are declared, but areBaseTypesDeclared reports it as not declared")
	}
	if !areBaseTypesDeclared(cpp_types.ArrayType(foo_type, 1), declared_types) {
		t.Error("Directly referred arrays of defined types are considred declared, but areBaseTypesDeclared reports them as not declared")
	}
	if !areBaseTypesDeclared(cpp_types.ArrayType(bar_type, 1), declared_types) {
		t.Error("Directly referred arrays of defined types are considred declared, but areBaseTypesDeclared reports them as not declared")
	}
	if !areBaseTypesDeclared(cpp_types.PointerType(bar_type), declared_types) {
		t.Error("Indirectly referred types are considered declared, but areBaseTypesDeclared reports them as not declared")
	}
}
