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

package vulkan_xml

import (
	"berberis/cpp_types"
	"fmt"
	"testing"
)

func TestUnmarshallNameAsString(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
                <type>This type is <name>int</name></type>
				<type category="test">Here is <name>type_name</name></type>
			</types>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if registry.Types[0].Name != "int" {
		t.Error("Name of type improperly parsed")
	}
	if registry.Types[1].Name != "type_name" {
		t.Error("Name of type improperly parsed")
	}
}

func TestUnknownType(t *testing.T) {
	_, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type name="Foo"/>
			</types>
		</registry>`))
	if err == nil {
		t.Error("XML was errorneously accepted")
		return
	}
	if err.Error() != "Unknown type without category: \"Foo\"" {
		t.Error("Unexpected error: \"" + err.Error() + "\"")
	}
}

func TestStructType(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="union" name="Union">
					<member><type>float</type> <name>float32</name>[4]</member>
					<member>const <type>int32_t</type> <name>int32</name>[4]</member>
				</type>
			</types>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if registry.Types[0].Members[0].Name != "float32" {
		t.Error("Name of member improperly parsed")
	}
	if registry.Types[0].Members[1].Name != "int32" {
		t.Error("Name of member improperly parsed")
	}

}

func TestEnumWarts(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<enums name="API Constants">
				<enum value="1000.0f"
				      name="VK_LOD_CLAMP_NONE"
				      comment="We don't care about this enum value but our parser shouldn't choke on it"/>
				<enum value="0"
				      name="VK_FALSE"
				      comment="We don't care about thus enum value but our parser shouldn't choke on it"/>
			</enums>
			<feature api="vulkan" name="VK_VERSION_1_0" number="1.0">
				<require comment="API constants">
					<enum name="VK_FALSE"/>
				</require>
			</feature>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, _, _, _, _, err = VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
}

func TestEnumExtend(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type name="VkStructureType" category="enum"/>
			</types>
			<feature api="vulkan" name="VK_VERSION_1_1" number="1.1">
				<require>
					<enum extends="VkStructureType" extnumber="158" offset="1" name="VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO"/>
				</require>
			</feature>
			<extensions>
				<extension name="VK_KHR_swapchain" number="2">
					<require>
						<enum value="70" name="VK_KHR_SWAPCHAIN_SPEC_VERSION"/>
						<enum offset="1" extends="VkStructureType" name="VK_STRUCTURE_TYPE_PRESENT_INFO_KHR"/>
					</require>
				</extension>
			</extensions>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, extensions, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if extension_spec, ok := extensions["VK_KHR_swapchain"]; ok {
		if extension_spec != 70 {
			t.Error("VK_KHR_swapchain spec is not 70")
		}
	} else {
		t.Error("VK_KHR_swapchain extension not found")
	}
	if typе, ok := types["VkStructureType"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Enum {
				t.Error("VkStructureType is not enum")
			}
			if typе.Elem(arch).Kind(arch) != cpp_types.Int32T {
				t.Error("VkStructureType is not std::int32_t-based enum")
			}
			if typе.NumField(arch) != 2 {
				t.Error("VkStructureType is supposed to have two fields")
			}
			enumerator := typе.Field(0, arch).(cpp_types.EnumFieldInfo)
			if enumerator.Name() != "VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO" {
				t.Error("VkStructureType's first element is not VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO")
			}
			if enumerator.Value() != 1000157001 {
				t.Error("VkStructureType's first element is not 1000157001")
			}
			enumerator = typе.Field(1, arch).(cpp_types.EnumFieldInfo)
			if enumerator.Name() != "VK_STRUCTURE_TYPE_PRESENT_INFO_KHR" {
				t.Error("VkStructureType's first element is not VK_STRUCTURE_TYPE_PRESENT_INFO_KHR")
			}
			if enumerator.Value() != 1000001001 {
				t.Error("VkStructureType's first element is not 1000001001")
			}
		}
		return
	}
	t.Error("VkStructureType type wasn't parsed")
}

func TestEnumUInt32(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type name="VkImageCreateFlagBits" category="enum"/>
			</types>
			<enums name="VkImageCreateFlagBits" type="enum">
				<enum bitpos="31"
				      name="VK_IMAGE_RESERVED_31_BIT"
				      comment="Note: that value doesn't fit into int32_t but does fit into uint32_t"/>
			</enums>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if typе, ok := types["VkImageCreateFlagBits"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Enum {
				t.Error("VkImageCreateFlagBits is not enum")
			}
			if typе.Elem(arch).Kind(arch) != cpp_types.UInt32T {
				t.Error("VkImageCreateFlagBits is not std::uint32_t-based enum")
			}
			if typе.NumField(arch) != 1 {
				t.Error("VkImageCreateFlagBits is supposed to have two fields")
			}
			enumerator := typе.Field(0, arch).(cpp_types.EnumFieldInfo)
			if enumerator.Name() != "VK_IMAGE_RESERVED_31_BIT" {
				t.Error("VkImageCreateFlagBits's first element is not VK_IMAGE_RESERVED_31_BIT")
			}
			if enumerator.Value() != 0x80000000 {
				t.Error("VkImageCreateFlagBits's first element is not 0x80000000")
			}
		}
		return
	}
	t.Error("VkImageCreateFlagBits type wasn't parsed")
}

func TestEnum64bit(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type name="Vk64BitEnum" category="enum"/>
			</types>
			<enums name="Vk64BitEnum" type="enum">
				<enum value="0x100000000" name="VK_64_BIT_ENUM_TEST_VALUE"/>
			</enums>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if typе, ok := types["Vk64BitEnum"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Enum {
				t.Error("Vk64BitEnum is not enum")
			}
			if typе.Elem(arch).Kind(arch) != cpp_types.Int64T {
				t.Error("Vk64BitEnum is not std::int64_t-based enum")
			}
			if typе.NumField(arch) != 1 {
				t.Error("Vk64BitEnum is supposed to have one field")
			}
			enumerator := typе.Field(0, arch).(cpp_types.EnumFieldInfo)
			if enumerator.Name() != "VK_64_BIT_ENUM_TEST_VALUE" {
				t.Error("Vk64BitEnum's first element is not VK_64_BIT_ENUM_TEST_VALUE")
			}
			if enumerator.Value() != 0x100000000 {
				t.Error("Vk64BitEnum's first element is not 0x100000000")
			}
		}
		return
	}
	t.Error("Vk64BitEnum type wasn't parsed")
}

func TestHandle(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="handle"><type>VK_DEFINE_HANDLE</type>(<name>VkInstance</name>)</type>
			</types>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if typе, ok := types["VkInstance"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Alias {
				t.Error("vkInstance is not alias")
				continue
			}
			if typе.Elem(arch).Kind(arch) != cpp_types.Ptr {
				t.Error("vkInstance is not pointer alias")
				continue
			}
			if typе.Elem(arch).Elem(arch).Kind(arch) != cpp_types.Opaque {
				t.Error("vkInstance is not pointer alias to opaque type")
				continue
			}
		}
		return
	}
	t.Error("vkInstance type wasn't parsed")
}

func TestNondispatchableHandle(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="handle"><type>VK_DEFINE_NON_DISPATCHABLE_HANDLE</type>(<name>VkFence</name>)</type>
			</types>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if typе, ok := types["VkFence"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Alias {
				t.Error("VkFence is not alias")
				continue
			}
			if arch%2 == 0 {
				if typе.Elem(arch) != cpp_types.UInt64TType {
					t.Error("VkFence is not uint64_t alias")
					continue
				}
			} else {
				if typе.Elem(arch).Kind(arch) != cpp_types.Ptr {
					t.Error("VkFence is not pointer alias")
					continue
				}
				if typе.Elem(arch).Elem(arch).Kind(arch) != cpp_types.Opaque {
					t.Error("VkFence is not pointer alias to opaque type")
					continue
				}
			}
		}
		return
	}
	t.Error("VkFence type wasn't parsed")
}

func TestFuncPtr(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="funcpointer">typedef void (VKAPI_PTR *<name>PFN_vkVoidFunction</name>)(void);</type>
				<type category="funcpointer" requires="VkDebugUtilsMessengerCallbackDataEXT">
					typedef uint32_t (VKAPI_PTR *<name>PFN_vkDebugUtilsMessengerCallbackEXT</name>)(
						<type>uint8_t</type>                                          pIndex,
						const <type>VkDebugUtilsMessengerCallbackDataEXT</type>*      pCallbackData,
						<type>void</type>*                                            pUserData);
				</type>
			</types>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if typе, ok := types["PFN_vkVoidFunction"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Ptr ||
				typе.Elem(arch).Kind(arch) != cpp_types.Func {
				t.Error("PFN_vkVoidFunction is not function pointer")
				continue
			}
			if typе.Elem(arch).Elem(arch).Kind(arch) != cpp_types.Void {
				t.Error("PFN_vkVoidFunction return type is not void")
			}
			if typе.Elem(arch).NumField(arch) != 0 {
				t.Error("PFN_vkVoidFunction is not zero-argument function")
			}
		}
	} else {
		t.Error("PFN_vkVoidFunction type wasn't parsed")
	}
	if typе, ok := types["PFN_vkDebugUtilsMessengerCallbackEXT"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Ptr ||
				typе.Elem(arch).Kind(arch) != cpp_types.Func {
				t.Error("VkDebugUtilsMessengerCallbackDataEXT is not function pointer")
				continue
			}
			if typе.Elem(arch).Elem(arch).Kind(arch) != cpp_types.UInt32T {
				t.Error("VkDebugUtilsMessengerCallbackDataEXT return type is not uint32_t")
			}
			if typе.Elem(arch).NumField(arch) != 3 {
				t.Error("VkDebugUtilsMessengerCallbackDataEXT is not two-argument function")
			}
			field0 := typе.Elem(arch).Field(0, arch)
			if field0.Name() != "pIndex" {
				t.Error("First argument of VkDebugUtilsMessengerCallbackDataEXT is not pIndex")
			}
			if field0.Type().Kind(arch) != cpp_types.UInt8T {
				t.Error("First argument of VkDebugUtilsMessengerCallbackDataEXT has wrong type")
			}
			field1 := typе.Elem(arch).Field(1, arch)
			if field1.Name() != "pCallbackData" {
				t.Error("Second argument of VkDebugUtilsMessengerCallbackDataEXT is not pCallbackData")
			}
			if field1.Type().Kind(arch) != cpp_types.Ptr ||
				field1.Type().Elem(arch).Kind(arch) != cpp_types.Const ||
				field1.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Opaque ||
				field1.Type().Elem(arch).Elem(arch).Name(arch) != "VkDebugUtilsMessengerCallbackDataEXT" {
				t.Error("Second argument of VkDebugUtilsMessengerCallbackDataEXT has wrong type")
			}
			field2 := typе.Elem(arch).Field(2, arch)
			if field2.Name() != "pUserData" {
				t.Error("Third argument of VkDebugUtilsMessengerCallbackDataEXT is not pUserData")
			}
			if field2.Type().Kind(arch) != cpp_types.Ptr ||
				field2.Type().Elem(arch).Kind(arch) != cpp_types.Void {
				t.Error("Third argument of VkDebugUtilsMessengerCallbackDataEXT has wrong type")
			}
		}
	} else {
		t.Error("PFN_vkDebugUtilsMessengerCallbackEXT type wasn't parsed")
	}
}

func TestStruct(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="basetype">typedef <type>uint32_t</type> <name>VkFlags</name>;</type>
				<type name="VkStructureType" category="enum"/>
				<type requires="VkGeometryInstanceFlagBitsKHR" category="bitmask">typedef <type>VkFlags</type> <name>VkGeometryInstanceFlagsKHR</name>;</type>
				<type category="struct" name="VkBaseOutStructure">
					<member><type>VkStructureType</type> <name>sType</name></member>
					<member>struct <type>VkBaseOutStructure</type>* <name>pNext</name></member>
				</type>
				<type category="struct" name="VkBaseInStructure">
					<member><type>VkStructureType</type> <name>sType</name></member>
					<member>const struct <type>VkBaseInStructure</type>* <name>pNext</name></member>
				</type>
				<type category="struct" name="VkAccelerationStructureInstanceKHR">
					<comment>The bitfields in this structure are non-normative since bitfield ordering is implementation-defined in C. The specification defines the normative layout.</comment>
					<member><type>VkTransformMatrixKHR</type> <name>transform</name></member>
					<member><type>uint32_t</type> <name>instanceCustomIndex</name>:24</member>
					<member><type>uint32_t</type> <name>mask</name>:8</member>
					<member><type>uint32_t</type> <name>instanceShaderBindingTableRecordOffset</name>:24</member>
					<member optional="true"><type>VkGeometryInstanceFlagsKHR</type> <name>flags</name>:8</member>
					<member><type>uint64_t</type> <name>accelerationStructureReference</name></member>
				</type>
				<type category="struct" name="VkTransformMatrixKHR">
					<member><type>float</type> <name>matrix</name>[3][4]</member>
				</type>
			</types>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, types, _, _, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if typе, ok := types["VkBaseOutStructure"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Struct {
				t.Error("VkBaseOutStructure is not a struct")
				continue
			}
			if typе.NumField(arch) != 2 {
				t.Error("VkBaseOutStructure is not two-field struct")
				continue
			}
			field0 := typе.Field(0, arch)
			if field0.Name() != "sType" {
				t.Error("First field of VkBaseOutStructure is not sType")
			}
			if field0.Type().Kind(arch) != cpp_types.Enum ||
				field0.Type().Elem(arch).Kind(arch) != cpp_types.Int32T {
				t.Error("First field of VkBaseOutStructure has wrong type")
			}
			field1 := typе.Field(1, arch)
			if field1.Name() != "pNext" {
				t.Error("Second field of VkBaseOutStructure is not pNext")
			}
			if field1.Type().Kind(arch) != cpp_types.Ptr ||
				field1.Type().Elem(arch).Kind(arch) != cpp_types.Struct {
				t.Error("Second field of VkBaseOutStructure has wrong type")
			}
		}
	} else {
		t.Error("VkBaseOutStructure type wasn't parsed")
	}
	if typе, ok := types["VkBaseInStructure"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Struct {
				t.Error("VkBaseInStructure is not a struct")
				continue
			}
			if typе.NumField(arch) != 2 {
				t.Error("VkBaseInStructure is not two-field struct")
			}
			field0 := typе.Field(0, arch)
			if field0.Name() != "sType" {
				t.Error("First field of VkBaseInStructure is not sType")
			}
			if field0.Type().Kind(arch) != cpp_types.Enum ||
				field0.Type().Elem(arch).Kind(arch) != cpp_types.Int32T {
				t.Error("First field of VkBaseInStructure has wrong type")
			}
			field1 := typе.Field(1, arch)
			if field1.Name() != "pNext" {
				t.Error("Second field of VkBaseInStructuree is not pNext")
			}
			if field1.Type().Kind(arch) != cpp_types.Ptr ||
				field1.Type().Elem(arch).Kind(arch) != cpp_types.Const ||
				field1.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Struct {
				t.Error("Second field of VkBaseInStructure has wrong type")
			}
		}
	} else {
		t.Error("VkBaseInStructure type wasn't parsed")
	}
	if typе, ok := types["VkTransformMatrixKHR"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Struct {
				t.Error("VkTransformMatrixKHR is not a struct")
				continue
			}
			if typе.NumField(arch) != 1 {
				t.Error("VkTransformMatrixKHR is not one-field struct")
			}
			field0 := typе.Field(0, arch)
			if field0.Name() != "matrix" {
				t.Error("First field of VkTransformMatrixKHR is not matrix")
			}
			if field0.Type().Kind(arch) != cpp_types.Array ||
				field0.Type().NumField(arch) != 4 ||
				field0.Type().Elem(arch).Kind(arch) != cpp_types.Array ||
				field0.Type().Elem(arch).NumField(arch) != 3 ||
				field0.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Float32 {
				t.Error("First field of VkTransformMatrixKHR has wrong type")
			}
		}
	} else {
		t.Error("PFN_vkDebugUtilsMessengerCallbackEXT type wasn't parsed")
	}
	if typе, ok := types["VkAccelerationStructureInstanceKHR"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if typе.Kind(arch) != cpp_types.Struct {
				t.Error("VkAccelerationStructureInstanceKHR is not a struct")
				continue
			}
			if typе.NumField(arch) != 6 {
				t.Error("VkAccelerationStructureInstanceKHR is not six-field struct")
			}
			assertLayout := func(t *testing.T, field_no uint, field cpp_types.FieldInfo, field_name string, size, offset uint) {
				if field.Name() != field_name {
					t.Error(fmt.Sprintf("Field %d of VkAccelerationStructureInstanceKHR is not %s", field_no, field_name))
				}
				if field.Type().Bits(arch) != size {
					t.Error(fmt.Sprintf("Field %d of VkAccelerationStructureInstanceKHR size is not %d bit", field_no, size))
				}
				if field.(cpp_types.StructFieldInfo).Offset() != offset {
					t.Error(fmt.Sprintf("Field %d of VkAccelerationStructureInstanceKHR offset is not %d bit", field_no, offset))
				}
			}
			field0 := typе.Field(0, arch)
			assertLayout(t, 1, field0, "transform", 384, 0)
			if field0.Type().Kind(arch) != cpp_types.Struct ||
				field0.Type().Name(arch) != "struct VkTransformMatrixKHR" {
				t.Error("First field of VkAccelerationStructureInstanceKHR has wrong type")
			}
			// Note: bitfields maybe represented differently but we don't care as long as
			// size and offsets match our expectations.
			field1 := typе.Field(1, arch)
			assertLayout(t, 2, field1, "instanceCustomIndex", 24, 384)
			field2 := typе.Field(2, arch)
			assertLayout(t, 3, field2, "mask", 8, 408)
			field3 := typе.Field(3, arch)
			assertLayout(t, 4, field3, "instanceShaderBindingTableRecordOffset", 24, 416)
			field4 := typе.Field(4, arch)
			assertLayout(t, 5, field4, "flags", 8, 440)
			field5 := typе.Field(5, arch)
			assertLayout(t, 6, field5, "accelerationStructureReference", 64, 448)
			if field5.Type().Kind(arch) != cpp_types.UInt64T {
				t.Error("Sixth field of VkAccelerationStructureInstanceKHR wrong type")
			}
		}
	} else {
		t.Error("PFN_vkDebugUtilsMessengerCallbackEXT type wasn't parsed")
	}
}

func TestCommand(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="define">
#define <name>VK_DEFINE_HANDLE</name>(object) typedef struct object##_T* object;</type>
				<type category="handle" parent="VkCommandPool"><type>VK_DEFINE_HANDLE</type>(<name>VkCommandBuffer</name>)</type>
			</types>
			<commands>
				<command queues="graphics" renderpass="both" cmdbufferlevel="primary,secondary">
					<proto><type>void</type> <name>vkCmdSetBlendConstants</name></proto>
					<param externsync="true"><type>VkCommandBuffer</type> <name>commandBuffer</name></param>
					<param><type>uint32_t</type> <name>blendConstantsLen</name></param>
					<param len="blendConstantsLen">const <type>float</type> <name>blendConstants</name>[4]</param>
				</command>
			</commands>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	_, _, _, commands, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if command, ok := commands["vkCmdSetBlendConstants"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if command.Kind(arch) != cpp_types.Func {
				t.Error("vkCmdSetBlendConstants is not a function")
				continue
			}
			if command.Elem(arch).Kind(arch) != cpp_types.Void {
				t.Error("vkCmdSetBlendConstants is not a void function")
			}
			if command.NumField(arch) != 3 {
				t.Error("vkCmdSetBlendConstants is not three-argument function")
				continue
			}
			field0 := command.Field(0, arch)
			if field0.Name() != "commandBuffer" {
				t.Error("First argument of vkCmdSetBlendConstants is not commandBuffer")
			}
			if field0.Type().Kind(arch) != cpp_types.Alias ||
				field0.Type().Elem(arch).Kind(arch) != cpp_types.Ptr ||
				field0.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Opaque ||
				field0.Type().Elem(arch).Elem(arch).Name(arch) != "struct VkCommandBuffer_T" {
				t.Error("First field of vkCmdSetBlendConstants has wrong type")
			}
			field1 := command.Field(1, arch)
			if field1.Name() != "blendConstantsLen" {
				t.Error("Second argument of vkCmdSetBlendConstants is not blendConstants")
			}
			if field1.Type().Kind(arch) != cpp_types.UInt32T {
				t.Error("Second field of vkCmdSetBlendConstants has wrong type")
			}
			field2 := command.Field(2, arch)
			if field2.Name() != "blendConstants" {
				t.Error("Third argument of vkCmdSetBlendConstants is not blendConstants")
			}
			if field2.Type().Kind(arch) != cpp_types.Ptr ||
				field2.Type().Elem(arch).Kind(arch) != cpp_types.Const ||
				field2.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Float32 ||
				field2.BaseFieldInfo().(ExtendedFieldInfo).Length().Name() != field1.Name() ||
				field2.BaseFieldInfo().(ExtendedFieldInfo).Length().Type() != field1.Type() {
				t.Error("Third field of vkCmdSetBlendConstants has wrong type")
			}
		}
	} else {
		t.Error("vkCmdSetBlendConstants command wasn't parsed")
	}
}

func TestCommandWithComplexLen(t *testing.T) {
	registry, err := Unmarshal([]byte(`
		<?xml version="1.0" encoding="UTF-8"?>
		<registry>
			<types>
				<type category="define">
#define <name>VK_DEFINE_HANDLE</name>(object) typedef struct object##_T* object;</type>
				<type category="handle"><type>VK_DEFINE_HANDLE</type>(<name>VkDescriptorPool</name>)</type>
				<type category="handle"><type>VK_DEFINE_HANDLE</type>(<name>VkDescriptorSet</name>)</type>
				<type category="handle"><type>VK_DEFINE_HANDLE</type>(<name>VkDescriptorSetLayout</name>)</type>
				<type category="handle"><type>VK_DEFINE_HANDLE</type>(<name>VkDevice</name>)</type>
				<type category="handle"><type>VK_DEFINE_HANDLE</type>(<name>VkStructureType</name>)</type>
				<type category="struct" name="VkDescriptorSetAllocateInfo">
					<member values="VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO"><type>VkStructureType</type> <name>sType</name></member>
					<member optional="true">const <type>void</type>* <name>pNext</name></member>
					<member><type>VkDescriptorPool</type> <name>descriptorPool</name></member>
					<member><type>uint32_t</type> <name>descriptorSetCount</name></member>
					<member len="descriptorSetCount">const <type>VkDescriptorSetLayout</type>* <name>pSetLayouts</name></member>
				</type>
			</types>
			<commands>
				<command successcodes="VK_SUCCESS" errorcodes="VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_OUT_OF_DEVICE_MEMORY,VK_ERROR_FRAGMENTED_POOL,VK_ERROR_OUT_OF_POOL_MEMORY">
					<proto><type>void</type> <name>vkAllocateDescriptorSets</name></proto>
					<param><type>VkDevice</type> <name>device</name></param>
					<param externsync="pAllocateInfo-&gt;descriptorPool">const <type>VkDescriptorSetAllocateInfo</type>* <name>pAllocateInfo</name></param>
					<param len="pAllocateInfo-&gt;descriptorSetCount"><type>VkDescriptorSet</type>* <name>pDescriptorSets</name></param>
				</command>
			</commands>
		</registry>`))
	if err != nil {
		t.Error("Failed to parse test XML" + err.Error())
		return
	}
	_, types, _, commands, _, err := VulkanTypesfromXML(registry)
	if err != nil {
		t.Error("Failed to parse test XML")
		return
	}
	if command, ok := commands["vkAllocateDescriptorSets"]; ok {
		for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
			if command.Kind(arch) != cpp_types.Func {
				t.Error("vkAllocateDescriptorSets is not a function")
				continue
			}
			if command.Elem(arch).Kind(arch) != cpp_types.Void {
				t.Error("vkAllocateDescriptorSets is not a void function")
			}
			if command.NumField(arch) != 3 {
				t.Error("vkAllocateDescriptorSets is not three-argument function")
				continue
			}
			field0 := command.Field(0, arch)
			if field0.Name() != "device" {
				t.Error("First argument of vkAllocateDescriptorSets is not device")
			}
			if field0.Type().Kind(arch) != cpp_types.Alias ||
				field0.Type().Elem(arch).Kind(arch) != cpp_types.Ptr ||
				field0.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Opaque ||
				field0.Type().Elem(arch).Elem(arch).Name(arch) != "struct VkDevice_T" {
				t.Error("First field of vkAllocateDescriptorSets has wrong type")
			}
			field1 := command.Field(1, arch)
			if field1.Name() != "pAllocateInfo" {
				t.Error("Second argument of vkAllocateDescriptorSets is not pAllocateInfo")
			}
			if field1.Type().Kind(arch) != cpp_types.Ptr ||
				field1.Type().Elem(arch).Kind(arch) != cpp_types.Const ||
				field1.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Struct ||
				field1.Type().Elem(arch).Elem(arch).Name(arch) != "struct VkDescriptorSetAllocateInfo" {
				t.Error("First field of vkAllocateDescriptorSets has wrong type")
			}
			field2 := command.Field(2, arch)
			if field2.Name() != "pDescriptorSets" {
				t.Error("Third argument of vkAllocateDescriptorSets is not pDescriptorSets")
			}
			field1_3 := types["VkDescriptorSetAllocateInfo"].Field(3, arch)
			if field2.Type().Kind(arch) != cpp_types.Ptr ||
				field2.Type().Elem(arch).Kind(arch) != cpp_types.Alias ||
				field2.Type().Elem(arch).Elem(arch).Kind(arch) != cpp_types.Ptr ||
				field2.Type().Elem(arch).Elem(arch).Elem(arch).Kind(arch) != cpp_types.Opaque ||
				field2.Type().Elem(arch).Elem(arch).Elem(arch).Name(arch) != "struct VkDescriptorSet_T" ||
				field2.BaseFieldInfo().(ExtendedFieldInfo).Length().Name() != field1.Name() ||
				field2.BaseFieldInfo().(ExtendedFieldInfo).Length().Type() != field1.Type() ||
				field2.BaseFieldInfo().(ExtendedFieldInfo).NestedField().Name() != field1_3.Name() ||
				field2.BaseFieldInfo().(ExtendedFieldInfo).NestedField().Type() != field1_3.Type() {
				t.Error("Third field of vkAllocateDescriptorSets has wrong type")
			}
		}
	} else {
		t.Error("vkCmdSetBlendConstants command wasn't parsed")
	}
}
