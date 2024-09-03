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
	"berberis/vulkan_types"
	"encoding/xml"
	"errors"
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

// Vulkan XML includes one platform entity which has many sub entities.
// We need XMLName for root elements, but validity of all other entities
// is guaranteed because of XML attributes.
type registry struct {
	XMLName    xml.Name       `xml:"registry"`
	Platforms  []platformInfo `xml:"platforms>platform"`
	Tags       []tagInfo      `xml:"tags>tag"`
	Types      []typeInfo     `xml:"types>type"`
	Enums      []enumInfo     `xml:"enums"`
	Commands   []commandInfo  `xml:"commands>command"`
	Extensions []struct {
		Name     string `xml:"name,attr"`
		ID       int64  `xml:"number,attr"`
		Requires []struct {
			EnumFields []enumFieldInfo `xml:"enum"`
		} `xml:"require"`
	} `xml:"extensions>extension"`
	Features []struct {
		EnumFields []enumFieldInfo `xml:"enum"`
	} `xml:"feature>require"`
	Comments string `xml:"comment"`
}

type platformInfo struct {
	Name    string `xml:"name,attr"`
	Comment string `xml:"comment,attr"`
}

type tagInfo struct {
	Name    string `xml:"name,attr"`
	Author  string `xml:"author,attr"`
	Contact string `xml:"contact,attr"`
	Comment string `xml:"comment,attr"`
}

type typeInfo struct {
	Name          string                 `xml:"name,attr"`
	Category      string                 `xml:"category,attr"`
	Requires      string                 `xml:"requires,attr"`
	Alias         string                 `xml:"alias,attr"`
	Members       []structuralMemberInfo `xml:"member"`
	StructExtends string                 `xml:"structextends,attr"`
	RawXML        string                 `xml:",innerxml"`
	Comment       string                 `xml:"comment,attr"`
	Api           string                 `xml:"api,attr"`
}

type enumInfo struct {
	Name       string          `xml:"name,attr"`
	EnumFields []enumFieldInfo `xml:"enum"`
}

type enumFieldInfo struct {
	Name    string `xml:"name,attr"`
	Alias   string `xml:"alias,attr"`
	Value   string `xml:"value,attr"`
	BitPos  string `xml:"bitpos,attr"`
	Dir     string `xml:"dir,attr"`
	Extends string `xml:"extends,attr"`
	ExtID   int64  `xml:"extnumber,attr"`
	Offset  int64  `xml:"offset,attr"`
}

type commandInfo struct {
	Name  string `xml:"name,attr"`
	Alias string `xml:"alias,attr"`
	Proto struct {
		Type    string `xml:"type,attr"`
		RawXML  string `xml:",innerxml"`
		Comment string `xml:"comment,attr"`
	} `xml:"proto"`
	Params []structuralMemberInfo `xml:"param"`
	RawXML string                 `xml:",innerxml"`
}

type structuralMemberInfo struct {
	Name         string `xml:"name,attr"`
	Type         string `xml:"type,attr"`
	Value        string `xml:"values,attr"`
	Length       string `xml:"len,attr"`
	AltLength    string `xml:"altlen,attr"`
	Validstructs string `xml:"validstructs,attr"`
	RawXML       string `xml:",innerxml"`
	Comment      string `xml:"comment,attr"`
	Api          string `xml:"api,attr"`
}

type ExtendedStructInfo interface {
	ExtendedWith() []cpp_types.Type
	OptionalStruct() bool
	OptionalValue() string
}

func ExtendedStruct(base_struct cpp_types.Type, optional_struct bool, optional_value string) cpp_types.Type {
	return &extendedStruct{base_struct, []cpp_types.Type{}, optional_struct, optional_value}
}

type extendedStruct struct {
	cpp_types.Type
	extended_with   []cpp_types.Type
	optional_struct bool
	optional_value  string
}

func (extended_struct *extendedStruct) ExtendedWith() []cpp_types.Type {
	return extended_struct.extended_with
}

func (extended_struct *extendedStruct) OptionalStruct() bool {
	return extended_struct.optional_struct
}

func (extended_struct *extendedStruct) OptionalValue() string {
	return extended_struct.optional_value
}

func ExtendedField(name string, typе cpp_types.Type, length, field cpp_types.FieldInfo) cpp_types.FieldInfo {
	return &extendedField{cpp_types.Field(name, typе), length, field}
}

type ExtendedFieldInfo interface {
	Length() cpp_types.FieldInfo
	NestedField() cpp_types.FieldInfo
}

type extendedField struct {
	cpp_types.FieldInfo
	length       cpp_types.FieldInfo
	nested_field cpp_types.FieldInfo
}

func (field_info *extendedField) BaseFieldInfo() cpp_types.FieldInfo {
	return field_info
}

func (field_info *extendedField) Length() cpp_types.FieldInfo {
	return field_info.length
}

func (field_info *extendedField) NestedField() cpp_types.FieldInfo {
	return field_info.nested_field
}

// Certain types come from platform files and vk.xml doesn't declare them.
// Not even category name is specified, so we have no idea if these are types
// or includes or anything else.
var known_types = map[string]string{
	"_screen_context":                      "screen/screen.h",
	"_screen_window":                       "screen/screen.h",
	"_screen_buffer":                       "screen/screen.h",
	"NvSciSyncAttrList":                    "nvscisync.h",
	"NvSciSyncObj":                         "nvscisync.h",
	"NvSciSyncFence":                       "nvscisync.h",
	"NvSciBufAttrList":                     "nvscibuf.h",
	"NvSciBufObj":                          "nvscibuf.h",
	"char":                                 "vk_platform",
	"Display":                              "X11/Xlib.h",
	"DWORD":                                "windows.h",
	"float":                                "vk_platform",
	"double":                               "vk_platform",
	"GgpFrameToken":                        "ggp_c/vulkan_types.h",
	"GgpStreamDescriptor":                  "ggp_c/vulkan_types.h",
	"HANDLE":                               "windows.h",
	"HINSTANCE":                            "windows.h",
	"HMONITOR":                             "windows.h",
	"HWND":                                 "windows.h",
	"IDirectFB":                            "directfb.h",
	"IDirectFBSurface":                     "directfb.h",
	"int":                                  "",
	"int8_t":                               "vk_platform",
	"int16_t":                              "vk_platform",
	"int32_t":                              "vk_platform",
	"int64_t":                              "vk_platform",
	"LPCWSTR":                              "windows.h",
	"RROutput":                             "X11/extensions/Xrandr.h",
	"SECURITY_ATTRIBUTES":                  "windows.h",
	"size_t":                               "vk_platform",
	"StdVideoDecodeH264Mvc":                "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH264MvcElement":         "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH264MvcElementFlags":    "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH264PictureInfo":        "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH264PictureInfoFlags":   "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH264ReferenceInfo":      "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH264ReferenceInfoFlags": "vk_video/vulkan_video_codec_h264std_decode.h",
	"StdVideoDecodeH265PictureInfo":        "vk_video/vulkan_video_codec_h265std_decode.h",
	"StdVideoDecodeH265PictureInfoFlags":   "vk_video/vulkan_video_codec_h265std_decode.h",
	"StdVideoDecodeH265ReferenceInfo":      "vk_video/vulkan_video_codec_h265std_decode.h",
	"StdVideoDecodeH265ReferenceInfoFlags": "vk_video/vulkan_video_codec_h265std_decode.h",
	"StdVideoEncodeH264PictureInfo":        "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264PictureInfoFlags":   "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264RefListModEntry":    "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264RefMemMgmtCtrlOperations":   "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264RefMgmtFlags":               "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264RefPicMarkingEntry":         "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264ReferenceInfo":              "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264ReferenceListsInfo":         "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264ReferenceInfoFlags":         "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264SliceHeader":                "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH264SliceHeaderFlags":           "vk_video/vulkan_video_codec_h264std_encode.h",
	"StdVideoEncodeH265PictureInfo":                "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265PictureInfoFlags":           "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265ReferenceInfo":              "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265ReferenceListsInfo":         "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265ReferenceInfoFlags":         "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265ReferenceModificationFlags": "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265ReferenceModifications":     "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265SliceHeader":                "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265SliceHeaderFlags":           "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265SliceSegmentHeader":         "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoEncodeH265SliceSegmentHeaderFlags":    "vk_video/vulkan_video_codec_h265std_encode.h",
	"StdVideoH264AspectRatioIdc":                   "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264CabacInitIdc":                     "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264ChromaFormatIdc":                  "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264DisableDeblockingFilterIdc":       "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264HrdParameters":                    "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264Level":                            "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264LevelIdc":                         "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264MemMgmtControlOp":                 "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264ModificationOfPicNumsIdc":         "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264PictureParameterSet":              "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264PictureType":                      "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264PocType":                          "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264PpsFlags":                         "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264ProfileIdc":                       "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264ScalingLists":                     "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264SequenceParameterSet":             "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264SequenceParameterSetVui":          "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264SliceType":                        "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264SpsFlags":                         "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264SpsVuiFlags":                      "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264WeightedBiPredIdc":                "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH264WeightedBipredIdc":                "vk_video/vulkan_video_codec_h264std.h",
	"StdVideoH265PictureParameterSet":              "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265DecPicBufMgr":                     "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265HrdFlags":                         "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265HrdParameters":                    "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265Level":                            "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265LevelIdc":                         "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265PictureType":                      "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265PpsFlags":                         "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265PredictorPaletteEntries":          "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265ProfileIdc":                       "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265ScalingLists":                     "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265SequenceParameterSet":             "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265SequenceParameterSetVui":          "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265SliceType":                        "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265SpsFlags":                         "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265SpsVuiFlags":                      "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265SubLayerHrdParameters":            "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265VideoParameterSet":                "vk_video/vulkan_video_codec_h265std.h",
	"StdVideoH265VpsFlags":                         "vk_video/vulkan_video_codec_h265std.h",
	"uint8_t":                                      "vk_platform",
	"uint16_t":                                     "vk_platform",
	"uint32_t":                                     "vk_platform",
	"uint64_t":                                     "vk_platform",
	"VisualID":                                     "X11/Xlib.h",
	"void":                                         "vk_platform",
	"Window":                                       "X11/Xlib.h",
	"wl_display":                                   "wayland-client.h",
	"wl_surface":                                   "wayland-client.h",
	"xcb_connection_t":                             "xcb/xcb.h",
	"xcb_visualid_t":                               "xcb/xcb.h",
	"xcb_window_t":                                 "xcb/xcb.h",
	"zx_handle_t":                                  "zircon/types.h",
}

// We don't really parse defines since we don't have a full-blown compiler.
// Instead we rely on the fact that there are very few defines in vk.xml and just verify that they match out assumptions.
var known_defines = map[string]string{
	"VK_MAKE_VERSION": "// DEPRECATED: This define is deprecated. VK_MAKE_API_VERSION should be used instead.\n" +
		"#define <name>VK_MAKE_VERSION</name>(major, minor, patch) \\\n" +
		"    ((((uint32_t)(major)) &lt;&lt; 22U) | (((uint32_t)(minor)) &lt;&lt; 12U) | ((uint32_t)(patch)))",
	"VK_VERSION_MAJOR": "// DEPRECATED: This define is deprecated. VK_API_VERSION_MAJOR should be used instead.\n" +
		"#define <name>VK_VERSION_MAJOR</name>(version) ((uint32_t)(version) &gt;&gt; 22U)",
	"VK_VERSION_MINOR": "// DEPRECATED: This define is deprecated. VK_API_VERSION_MINOR should be used instead.\n" +
		"#define <name>VK_VERSION_MINOR</name>(version) (((uint32_t)(version) &gt;&gt; 12U) &amp; 0x3FFU)",
	"VK_VERSION_PATCH": "// DEPRECATED: This define is deprecated. VK_API_VERSION_PATCH should be used instead.\n" +
		"#define <name>VK_VERSION_PATCH</name>(version) ((uint32_t)(version) &amp; 0xFFFU)",

	"VK_MAKE_API_VERSION": "#define <name>VK_MAKE_API_VERSION</name>(variant, major, minor, patch) \\\n" +
		"    ((((uint32_t)(variant)) &lt;&lt; 29U) | (((uint32_t)(major)) &lt;&lt; 22U) | (((uint32_t)(minor)) &lt;&lt; 12U) | ((uint32_t)(patch)))",
	"VK_API_VERSION_VARIANT": "#define <name>VK_API_VERSION_VARIANT</name>(version) ((uint32_t)(version) &gt;&gt; 29U)",
	"VK_API_VERSION_MAJOR":   "#define <name>VK_API_VERSION_MAJOR</name>(version) (((uint32_t)(version) &gt;&gt; 22U) &amp; 0x7FU)",
	"VK_API_VERSION_MINOR":   "#define <name>VK_API_VERSION_MINOR</name>(version) (((uint32_t)(version) &gt;&gt; 12U) &amp; 0x3FFU)",
	"VK_API_VERSION_PATCH":   "#define <name>VK_API_VERSION_PATCH</name>(version) ((uint32_t)(version) &amp; 0xFFFU)",

	"VKSC_API_VARIANT": "// Vulkan SC variant number\n#define <name>VKSC_API_VARIANT</name> 1",
	"VK_API_VERSION": "// DEPRECATED: This define has been removed. Specific version defines (e.g. VK_API_VERSION_1_0), or the VK_MAKE_VERSION macro, should be used instead.\n" +
		"//#define <name>VK_API_VERSION</name> <type>VK_MAKE_API_VERSION</type>(0, 1, 0, 0) // Patch version should always be set to 0",
	"VK_API_VERSION_1_0": "// Vulkan 1.0 version number\n" +
		"#define <name>VK_API_VERSION_1_0</name> <type>VK_MAKE_API_VERSION</type>(0, 1, 0, 0)// Patch version should always be set to 0",
	"VK_API_VERSION_1_1": "// Vulkan 1.1 version number\n" +
		"#define <name>VK_API_VERSION_1_1</name> <type>VK_MAKE_API_VERSION</type>(0, 1, 1, 0)// Patch version should always be set to 0",
	"VK_API_VERSION_1_2": "// Vulkan 1.2 version number\n" +
		"#define <name>VK_API_VERSION_1_2</name> <type>VK_MAKE_API_VERSION</type>(0, 1, 2, 0)// Patch version should always be set to 0",
	"VK_API_VERSION_1_3": "// Vulkan 1.3 version number\n" +
		"#define <name>VK_API_VERSION_1_3</name> <type>VK_MAKE_API_VERSION</type>(0, 1, 3, 0)// Patch version should always be set to 0",
	"VKSC_API_VERSION_1_0": "// Vulkan SC 1.0 version number\n#define <name>VKSC_API_VERSION_1_0</name> <type>VK_MAKE_API_VERSION</type>(VKSC_API_VARIANT, 1, 0, 0)// Patch version should always be set to 0",
	"VK_HEADER_VERSION": "// Version of this file\n" +
		"#define <name>VK_HEADER_VERSION</name> ",
	"VK_HEADER_VERSION_COMPLETE": "// Complete version of this file\n" +
		"#define <name>VK_HEADER_VERSION_COMPLETE</name> <type>VK_MAKE_API_VERSION</type>",
	"VK_DEFINE_HANDLE": "\n#define <name>VK_DEFINE_HANDLE</name>(object) typedef struct object##_T* object;",
	"VK_USE_64_BIT_PTR_DEFINES": "\n" +
		"#ifndef VK_USE_64_BIT_PTR_DEFINES\n" +
		"    #if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) &amp;&amp; !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__) || (defined(__riscv) &amp;&amp; __riscv_xlen == 64)\n" +
		"        #define VK_USE_64_BIT_PTR_DEFINES 1\n" +
		"    #else\n" +
		"        #define VK_USE_64_BIT_PTR_DEFINES 0\n" +
		"    #endif\n" +
		"#endif",
	"VK_NULL_HANDLE": "\n" +
		"#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE\n" +
		"    #if (VK_USE_64_BIT_PTR_DEFINES==1)\n" +
		"        #if (defined(__cplusplus) &amp;&amp; (__cplusplus >= 201103L)) || (defined(_MSVC_LANG) &amp;&amp; (_MSVC_LANG >= 201103L))\n" +
		"            #define VK_NULL_HANDLE nullptr\n" +
		"        #else\n" +
		"            #define VK_NULL_HANDLE ((void*)0)\n" +
		"        #endif\n" +
		"    #else\n" +
		"        #define VK_NULL_HANDLE 0ULL\n" +
		"    #endif\n" +
		"#endif\n" +
		"#ifndef VK_NULL_HANDLE\n" +
		"    #define VK_NULL_HANDLE 0\n" +
		"#endif",
}

var vulkan_known_defines = map[string]string{
	"VK_DEFINE_HANDLE": "\n#define <name>VK_DEFINE_HANDLE</name>(object) typedef struct object##_T* object;",
	"VK_DEFINE_NON_DISPATCHABLE_HANDLE": "\n" +
		"#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE\n" +
		"    #if (VK_USE_64_BIT_PTR_DEFINES==1)\n" +
		"        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;\n" +
		"    #else\n" +
		"        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;\n" +
		"    #endif\n" +
		"#endif",
}

var vulkansc_known_defines = map[string]string{
	"VK_DEFINE_HANDLE": "\n#define <name>VK_DEFINE_HANDLE</name>(object) typedef struct object##_T* (object);",
	"VK_DEFINE_NON_DISPATCHABLE_HANDLE": "\n" +
		"#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE\n" +
		"    #if (VK_USE_64_BIT_PTR_DEFINES==1)\n" +
		"        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *(object);\n" +
		"    #else\n" +
		"        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t (object);\n" +
		"    #endif\n" +
		"#endif",
}

var known_defines_obsoleted = map[string]string{
	"VK_MAKE_VERSION": "// DEPRECATED: This define is deprecated. VK_MAKE_API_VERSION should be used instead.\n" +
		"#define <name>VK_MAKE_VERSION</name>(major, minor, patch) \\\n" +
		"    ((((uint32_t)(major)) &lt;&lt; 22) | (((uint32_t)(minor)) &lt;&lt; 12) | ((uint32_t)(patch)))",
	"VK_VERSION_MAJOR": "// DEPRECATED: This define is deprecated. VK_API_VERSION_MAJOR should be used instead.\n" +
		"#define <name>VK_VERSION_MAJOR</name>(version) ((uint32_t)(version) &gt;&gt; 22)",
	"VK_VERSION_MINOR": "// DEPRECATED: This define is deprecated. VK_API_VERSION_MINOR should be used instead.\n" +
		"#define <name>VK_VERSION_MINOR</name>(version) (((uint32_t)(version) &gt;&gt; 12) &amp; 0x3FFU)",
	"VK_VERSION_PATCH": "// DEPRECATED: This define is deprecated. VK_API_VERSION_PATCH should be used instead.\n" +
		"#define <name>VK_VERSION_PATCH</name>(version) ((uint32_t)(version) &amp; 0xFFFU)",
	"VK_MAKE_API_VERSION": "#define <name>VK_MAKE_API_VERSION</name>(variant, major, minor, patch) \\\n" +
		"    ((((uint32_t)(variant)) &lt;&lt; 29) | (((uint32_t)(major)) &lt;&lt; 22) | (((uint32_t)(minor)) &lt;&lt; 12) | ((uint32_t)(patch)))",
	"VKSC_API_VARIANT":       "// Vulkan SC variant number \n#define <name>VKSC_API_VARIANT</name> 1 // DEPRECATED: This define has been removed. Specific version defines (e.g. VK_API_VERSION_1_0), or the VK_MAKE_VERSION macro, should be used instead.",
	"VK_API_VERSION_VARIANT": "#define <name>VK_API_VERSION_VARIANT</name>(version) ((uint32_t)(version) &gt;&gt; 29)",
	"VK_API_VERSION_MAJOR":   "#define <name>VK_API_VERSION_MAJOR</name>(version) (((uint32_t)(version) &gt;&gt; 22) &amp; 0x7FU)",
	"VK_API_VERSION_MINOR":   "#define <name>VK_API_VERSION_MINOR</name>(version) (((uint32_t)(version) &gt;&gt; 12) &amp; 0x3FFU)",
	"VK_API_VERSION_PATCH":   "#define <name>VK_API_VERSION_PATCH</name>(version) ((uint32_t)(version) &amp; 0xFFFU)",
	"VK_API_VERSION": "// DEPRECATED: This define has been removed. Specific version defines (e.g. VK_API_VERSION_1_0), or the VK_MAKE_VERSION macro, should be used instead.\n" +
		"//#define <name>VK_API_VERSION</name> <type>VK_MAKE_VERSION</type>(1, 0, 0) // Patch version should always be set to 0",
	"VK_API_VERSION_1_0": "// Vulkan 1.0 version number\n" +
		"#define <name>VK_API_VERSION_1_0</name> <type>VK_MAKE_VERSION</type>(1, 0, 0)// Patch version should always be set to 0",
	"VK_API_VERSION_1_1": "// Vulkan 1.1 version number\n" +
		"#define <name>VK_API_VERSION_1_1</name> <type>VK_MAKE_VERSION</type>(1, 1, 0)// Patch version should always be set to 0",
	"VK_API_VERSION_1_2": "// Vulkan 1.2 version number\n" +
		"#define <name>VK_API_VERSION_1_2</name> <type>VK_MAKE_VERSION</type>(1, 2, 0)// Patch version should always be set to 0",
	"VKSC_API_VERSION_1_0": "VK_MAKE_API_VERSION</type>(VKSC_API_VARIANT, 1, 0, 0)// Patch version should always be set to 0",
	"VK_HEADER_VERSION": "// Version of this file\n" +
		"#define <name>VK_HEADER_VERSION</name> ",
	"VK_HEADER_VERSION_COMPLETE": "// Complete version of this file\n" +
		"#define <name>VK_HEADER_VERSION_COMPLETE</name> <type>VK_MAKE_VERSION</type>(1, 2, VK_HEADER_VERSION)",
	"VK_USE_64_BIT_PTR_DEFINES": "\n" +
		"#ifndef VK_USE_64_BIT_PTR_DEFINES\n" +
		"    #if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) &amp;&amp; !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)\n" +
		"        #define VK_USE_64_BIT_PTR_DEFINES 1\n" +
		"    #else\n" +
		"        #define VK_USE_64_BIT_PTR_DEFINES 0\n" +
		"    #endif\n" +
		"#endif",
	"VK_DEFINE_NON_DISPATCHABLE_HANDLE": "\n#if !defined(VK_DEFINE_NON_DISPATCHABLE_HANDLE)\n" +
		"#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) &amp;&amp; !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)\n" +
		"        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;\n" +
		"#else\n" +
		"        #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;\n" +
		"#endif\n" +
		"#endif",
	"VK_NULL_HANDLE":   "\n#define <name>VK_NULL_HANDLE</name> 0",
	"VK_DEFINE_HANDLE": "\n#define <name>VK_DEFINE_HANDLE</name>(object) typedef struct object##_T* (object);",
}

func Unmarshal(data []byte) (*registry, error) {
	var registry registry
	err := xml.Unmarshal(data, &registry)
	if err != nil {
		return nil, err
	}
	for i := range registry.Types {
		typе := &registry.Types[i]
		if typе.Name == "" {
			typе.Name, err = elementFromRawXML("name", typе.RawXML)
			if err != nil {
				return nil, err
			}
		}
		if typе.Category == "" {
			if requires, found := known_types[typе.Name]; !found || typе.Requires != requires {
				return nil, errors.New("Unknown type without category: \"" + typе.Name + "\"")
			}
			typе.Category = "vk_platform"
		}
		if typе.Alias != "" || (typе.Category != "struct" && typе.Category != "union") {
			if len(typе.Members) != 0 {
				return nil, errors.New("Members in non-struct type : \"" + typе.Name + "\"")
			}
		} else {
			for j := range typе.Members {
				member := &typе.Members[j]
				if member.Name == "" {
					member.Name, err = elementFromRawXML("name", member.RawXML)
					if err != nil {
						return nil, err
					}
				}
				if member.Type == "" {
					member.Type, err = elementFromRawXML("type", member.RawXML)
					if err != nil {
						return nil, err
					}
				}
				if member.Comment == "" && strings.Contains(member.RawXML, "<comment>") {
					member.Comment, err = elementFromRawXML("comment", member.RawXML)
					if err != nil {
						return nil, err
					}
				}
			}
		}
	}
	for i := range registry.Commands {
		command := &registry.Commands[i]
		if command.Name == "" {
			command.Name, err = elementFromRawXML("name", command.Proto.RawXML)
			if err != nil {
				return nil, err
			}
		}
		if command.Alias == "" {
			if command.Proto.Type == "" {
				command.Proto.Type, err = elementFromRawXML("type", command.Proto.RawXML)
				if err != nil {
					return nil, err
				}
			}
			if command.Proto.Comment == "" && strings.Contains(command.Proto.RawXML, "<comment>") {
				command.Proto.Type, err = elementFromRawXML("comment", command.Proto.RawXML)
				if err != nil {
					return nil, err
				}
			}
			for j := range command.Params {
				param := &command.Params[j]
				if param.Name == "" {
					param.Name, err = elementFromRawXML("name", param.RawXML)
					if err != nil {
						return nil, err
					}
				}
				if param.Type == "" {
					param.Type, err = elementFromRawXML("type", param.RawXML)
					if err != nil {
						return nil, err
					}
				}
				if param.Comment == "" && strings.Contains(command.Proto.RawXML, "<comment>") {
					param.Comment, err = elementFromRawXML("comment", param.RawXML)
					if err != nil {
						return nil, err
					}
				}
			}
		}
	}
	return &registry, nil
}

func VulkanTypesfromXML(registry *registry) (sorted_type_names []string, types map[string]cpp_types.Type, sorted_command_names []string, commands map[string]cpp_types.Type, extensions map[string]int64, err error) {
	types = vulkan_types.PlatformTypes()
	// Note that we don't pre-calculate values for enums during initial parsing because vk.xml
	// [ab]uses "enum" to define non-integer constants and integers defined-as-C-expression, too.
	// E.g. "VK_LOD_CLAMP_NONE" as "1000.0f" or VK_QUEUE_FAMILY_FOREIGN_EXT as "(~0U-2)".
	// We return "raw" string value here and only parse them on as-needed basis.
	enum_values, enum_types, err := parseEnumValues(registry)
	if err != nil {
		return nil, nil, nil, nil, nil, err
	}
	// It's allowed in C/C++ to refer to not-yet defined type and type may even include pointer
	// to itself. But other uses of undefined types are forbidden. Mutually directly (non-pointer)
	// used types are *forbidden* in C/C++.
	// Thus the graph of type uses is acyclic when pointers are excluded and we should be
	// able to iteratively resolve all types by retrying failed types, expecting at least one
	// type to resolve during an iteration.
	// The first loop parsing resolves all directly (non-pointer) used types.
	//
	// Pointed types just became opaque, if undefined, and will be attempted to be resolved with
	// a dedicated pass.
	// The second loop (below) replaces opaque types referenced by pointer if they have been
	// resolved at some point. Some types are supposed to just be opaque types and be only
	// operated using pointers — but only platform-provided types, not vk.xml-defined types.
	var xml_types_list []*typeInfo
	next_xml_types_list := []*typeInfo{}
	for index := range registry.Types {
		next_xml_types_list = append(next_xml_types_list, &registry.Types[index])
	}
	for len(next_xml_types_list) > 0 {
		// If next list is the same as previous one then we have some kind of loop and types couldn't be defined.
		if len(xml_types_list) == len(next_xml_types_list) {
			return nil, nil, nil, nil, nil, errors.New("Cannot make any progress: type \"" + xml_types_list[0].Name + "\" refers to undefined type: \"" + xml_types_list[0].RawXML + "\"\"")
		}
		xml_types_list = next_xml_types_list
		next_xml_types_list = []*typeInfo{}
		for _, xml_type := range xml_types_list {
			if _, ok := types[xml_type.Name]; ok {
				if xml_type.Category == "vk_platform" {
					continue
				}
				if xml_type.Api == "vulkansc" {
					continue
				}
				return nil, nil, nil, nil, nil, errors.New("Duplicated type \"" + xml_type.Name + "\"")
			}
			if xml_type.Alias != "" {
				if alias_target, ok := types[xml_type.Alias]; ok {
					types[xml_type.Name] = cpp_types.AliasType(xml_type.Name, alias_target)
					continue
				}
				next_xml_types_list = append(next_xml_types_list, xml_type)
				continue
			}
			var c_type cpp_types.Type
			switch xml_type.Category {
			case "basetype":
				c_type, err = vulkanBaseTypeFromXML(xml_type)
			case "bitmask":
				c_type, err = vulkanBitmaskTypeFromXML(xml_type, types)
			case "define":
				err := vulkanDefineTypeFromXML(xml_type)
				if err != nil {
					return nil, nil, nil, nil, nil, err
				}
				continue
			case "enum":
				c_type, err = vulkanEnumTypeFromXML(xml_type, enum_values, enum_types)
			case "funcpointer":
				c_type, err = vulkanFuncPoiterTypeFromXML(xml_type, types)
			case "handle":
				c_type, err = vulkanHandleTypeFromXML(xml_type, types)
			case "include":
				continue
			case "struct":
				c_type, err = vulkanStructTypeFromXML(xml_type, xml_type.StructExtends != "", types, enum_values)
			case "union":
				c_type, err = vulkanUnionTypeFromXML(xml_type, types, enum_values)
			case "vk_platform":
				return nil, nil, nil, nil, nil, errors.New("Unknown platform type \"" + xml_type.Name + "\"")
			default:
				return nil, nil, nil, nil, nil, errors.New("Unknown type category \"" + xml_type.Category + "\"")
			}
			// This type refers the unknown type. But it maybe because it needs some type defined further on in the xml. Defer its parsing to next pass.
			if err == unknownType {
				next_xml_types_list = append(next_xml_types_list, xml_type)
				continue
			}
			if err != nil {
				return nil, nil, nil, nil, nil, err
			}
			types[xml_type.Name] = c_type
		}
	}
	// Populate extended_with info. We need to be the separate path since structs may refer other structs which are defined later in the vk.xml file.
	for _, xml_type := range registry.Types {
		if xml_type.Category == "struct" && xml_type.StructExtends != "" {
			for _, name := range strings.Split(xml_type.StructExtends, ",") {
				var extended_with *[]cpp_types.Type
				if types[name].Kind(cpp_types.FirstArch) == cpp_types.Alias {
					extended_with = &types[name].Elem(cpp_types.FirstArch).(*extendedStruct).extended_with
				} else {
					extended_with = &types[name].(*extendedStruct).extended_with
				}
				*extended_with = append(*extended_with, types[xml_type.Name])
			}
		}
	}
	// Resolve potentially circular references.
	for type_name := range types {
		// Here we rely on the fact that there are no types in Vulkan which are stuctural in one case yet non-structural
		// in other cases. And also there are differently-structured structural types either.
		typе := types[type_name]
		if typе.Kind(cpp_types.FirstArch) == cpp_types.Ptr {
			typе = typе.Elem(cpp_types.FirstArch)
		}
		if typе.Kind(cpp_types.FirstArch) != cpp_types.Func &&
			typе.Kind(cpp_types.FirstArch) != cpp_types.Struct &&
			typе.Kind(cpp_types.FirstArch) != cpp_types.Union {
			continue
		}
		for i := uint(0); i < typе.NumField(cpp_types.FirstArch); i++ {
			field := typе.Field(i, cpp_types.FirstArch)
			if field.Type().Kind(cpp_types.FirstArch) != cpp_types.Ptr {
				continue
			}
			pointee_type := field.Type().Elem(cpp_types.FirstArch)
			if pointee_type.Kind(cpp_types.FirstArch) == cpp_types.Opaque {
				if field_type, ok := types[pointee_type.Name(cpp_types.FirstArch)]; ok && field_type.Kind(cpp_types.FirstArch) != cpp_types.Opaque {
					field.Type().(cpp_types.ModifyablePtrType).ReplaceElem(field_type)
				}
			} else if pointee_type.Kind(cpp_types.FirstArch) == cpp_types.Const &&
				pointee_type.Elem(cpp_types.FirstArch).Kind(cpp_types.FirstArch) == cpp_types.Opaque {
				if field_type, ok := types[pointee_type.Elem(cpp_types.FirstArch).Name(cpp_types.FirstArch)]; ok && field_type.Kind(cpp_types.FirstArch) != cpp_types.Opaque {
					field.Type().(cpp_types.ModifyablePtrType).ReplaceElem(cpp_types.ConstType(field_type))
				}
			}
		}
	}
	commands = make(map[string]cpp_types.Type)
	for index := range registry.Commands {
		command := registry.Commands[index]
		// We'll link aliases below, after the final commands are constructed.
		if command.Alias != "" {
			continue
		}
		if result_type, ok := types[command.Proto.Type]; ok {
			if space.ReplaceAllString(command.Proto.RawXML, " ") != fmt.Sprintf("<type>%s</type> <name>%s</name>", command.Proto.Type, command.Name) {
				return nil, nil, nil, nil, nil, errors.New("Unexpected prototype \"" + command.Proto.RawXML + "\"")
			}
			fields_info, err := vulkanStructuralTypeMembersFromXML(command.Name, command.Params, types, enum_values)
			if err != nil {
				return nil, nil, nil, nil, nil, err
			}
			// Arrays decay into pointers when used as parameters of functions - but only one, outer, dimension.
			// Convert arrays into pointers, leave other types intact.
			params_info := []cpp_types.FieldInfo{}
			for _, field_info := range fields_info {
				if field_info.Type().Kind(cpp_types.FirstArch) == cpp_types.Array {
					params_info = append(params_info, ExtendedField(
						field_info.Name(),
						cpp_types.PointerType(field_info.Type().Elem(cpp_types.FirstArch)),
						field_info.BaseFieldInfo().(ExtendedFieldInfo).Length(),
						field_info.BaseFieldInfo().(ExtendedFieldInfo).NestedField()))
				} else {
					params_info = append(params_info, field_info)
				}
			}
			commands[command.Name] = cpp_types.FunctionType(result_type, params_info)
			continue
		}
		return nil, nil, nil, nil, nil, errors.New("Unknown return function type \"" + command.Proto.Type + "\"")
	}
	// Now link aliases to the final command of the original function.
	for index := range registry.Commands {
		command := registry.Commands[index]
		if command.Alias == "" {
			continue
		}
		commands[command.Name] = commands[command.Alias]
	}
	sorted_type_names = []string{}
	for name := range types {
		sorted_type_names = append(sorted_type_names, name)
	}
	sort.Strings(sorted_type_names)
	sorted_command_names = []string{}
	for name := range commands {
		sorted_command_names = append(sorted_command_names, name)
	}
	sort.Strings(sorted_command_names)
	extensions = make(map[string]int64)
	for extension_idx := range registry.Extensions {
		extension := &registry.Extensions[extension_idx]
		extensions_spec := int64(-1)
		for requires_idx := range extension.Requires {
			requires := &extension.Requires[requires_idx]
			for enum_field_idx := range requires.EnumFields {
				enum_field := &requires.EnumFields[enum_field_idx]
				if enum_field.Alias == "" && strings.HasSuffix(enum_field.Name, "_SPEC_VERSION") {
					spec_version, err := strconv.ParseInt(enum_field.Value, 10, 32)
					if err != nil {
						return nil, nil, nil, nil, nil, err
					}
					if spec_version == -1 || extensions_spec != -1 {
						errors.New("Couldn't find extensions SPEC_VERSION")
					}
					extensions_spec = spec_version
				}
			}
		}
		extensions[extension.Name] = extensions_spec
	}
	return sorted_type_names, types, sorted_command_names, commands, extensions, nil
}

func vulkanBaseTypeFromXML(typе *typeInfo) (cpp_types.Type, error) {
	RawXML := strings.TrimSpace(space.ReplaceAllString(typе.RawXML, " "))
	if typе.Name == "CAMetalLayer" {
		if RawXML != "#ifdef __OBJC__ @class CAMetalLayer; #else typedef void <name>CAMetalLayer</name>; #endif" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.OpaqueType("CAMetalLayer"), nil
	}
	if typе.Name == "MTLDevice_id" {
		if RawXML != "#ifdef __OBJC__ @protocol MTLDevice; typedef id&lt;MTLDevice&gt; MTLDevice_id; #else typedef void* <name>MTLDevice_id</name>; #endif" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.PointerType(cpp_types.VoidType), nil
	}
	if typе.Name == "MTLCommandQueue_id" {
		if RawXML != "#ifdef __OBJC__ @protocol MTLCommandQueue; typedef id&lt;MTLCommandQueue&gt; MTLCommandQueue_id; #else typedef void* <name>MTLCommandQueue_id</name>; #endif" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.PointerType(cpp_types.VoidType), nil
	}
	if typе.Name == "MTLBuffer_id" {
		if RawXML != "#ifdef __OBJC__ @protocol MTLBuffer; typedef id&lt;MTLBuffer&gt; MTLBuffer_id; #else typedef void* <name>MTLBuffer_id</name>; #endif" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.PointerType(cpp_types.VoidType), nil
	}
	if typе.Name == "MTLTexture_id" {
		if RawXML != "#ifdef __OBJC__ @protocol MTLTexture; typedef id&lt;MTLTexture&gt; MTLTexture_id; #else typedef void* <name>MTLTexture_id</name>; #endif" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.PointerType(cpp_types.VoidType), nil
	}
	if typе.Name == "MTLSharedEvent_id" {
		if RawXML != "#ifdef __OBJC__ @protocol MTLSharedEvent; typedef id&lt;MTLSharedEvent&gt; MTLSharedEvent_id; #else typedef void* <name>MTLSharedEvent_id</name>; #endif" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.PointerType(cpp_types.VoidType), nil
	}
	if typе.Name == "IOSurfaceRef" {
		if RawXML != "typedef struct __IOSurface* <name>IOSurfaceRef</name>;" {
			return nil, errors.New("Unexpected define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return cpp_types.PointerType(cpp_types.OpaqueType("__IOSurface")), nil
	}
	if RawXML == fmt.Sprintf("struct <name>%s</name>;", typе.Name) {
		return cpp_types.OpaqueType(typе.Name), nil
	}
	if RawXML == fmt.Sprintf("typedef <type>uint32_t</type> <name>%s</name>;", typе.Name) {
		return cpp_types.AliasType(typе.Name, cpp_types.UInt32TType), nil
	}
	if RawXML == fmt.Sprintf("typedef <type>uint64_t</type> <name>%s</name>;", typе.Name) {
		return cpp_types.AliasType(typе.Name, cpp_types.UInt64TType), nil
	}
	if RawXML == fmt.Sprintf("typedef <type>void</type>* <name>%s</name>;", typе.Name) {
		return cpp_types.AliasType(typе.Name, cpp_types.PointerType(cpp_types.VoidType)), nil
	}
	return nil, errors.New("Unexpected basetype \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
}

func vulkanBitmaskTypeFromXML(typе *typeInfo, types map[string]cpp_types.Type) (cpp_types.Type, error) {
	RawXML := strings.TrimSpace(space.ReplaceAllString(typе.RawXML, " "))
	if RawXML == fmt.Sprintf("typedef <type>VkFlags</type> <name>%s</name>;", typе.Name) {
		return cpp_types.AliasType(typе.Name, types["VkFlags"]), nil
	}
	if RawXML == fmt.Sprintf("typedef <type>VkFlags64</type> <name>%s</name>;", typе.Name) {
		return cpp_types.AliasType(typе.Name, types["VkFlags64"]), nil
	}
	return nil, errors.New("Unexpected bitmask \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
}

func vulkanDefineTypeFromXML(typе *typeInfo) error {
	if typе.Api == "vulkan" {
		if define, ok := vulkan_known_defines[typе.Name]; ok {
			if define != typе.RawXML {
				return errors.New("Unmatched define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
			}
			return nil
		}
	} else if typе.Api == "vulkansc" {
		if define, ok := vulkansc_known_defines[typе.Name]; ok {
			if define != typе.RawXML {
				return errors.New("Unmatched define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
			}
			return nil
		}
	} else {
		if define, ok := vulkan_known_defines[typе.Name]; ok {
			if define == typе.RawXML {
				return nil
			}
		}
		if define, ok := vulkansc_known_defines[typе.Name]; ok {
			if define != typе.RawXML {
				return errors.New("Unmatched define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
			}
			return nil
		}
	}
	if define, ok := known_defines[typе.Name]; ok {
		// Most defines are stable and since we don't parse them we just ensure they match our expectations.
		if typе.Name != "VK_HEADER_VERSION" && typе.Name != "VK_HEADER_VERSION_COMPLETE" {
			if define != typе.RawXML {
				if obsolete_define, ok := known_defines_obsoleted[typе.Name]; ok {
					if obsolete_define == typе.RawXML {
						return nil
					}
					return errors.New("Unmatched define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
				}
				return errors.New("Unknown obsolete define \"" + typе.Name + "\"")
			}
			return nil
		}
		// Note: VK_HEADER_VERSION is updated every time vk.xml is updated thus we couldn't hardcode it.
		// VK_HEADER_VERSION_COMPLETE is updted when new, incompatible version of Vulkan is released.
		if !strings.HasPrefix(typе.RawXML, define) {
			return errors.New("Unmatched define \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
		}
		return nil
	}
	return errors.New("Unknown define \"" + typе.Name + "\"")
}

func vulkanEnumTypeFromXML(typе *typeInfo, enum_values map[string]*enumFieldInfo, enum_types map[string][]*enumFieldInfo) (cpp_types.Type, error) {
	fits_in_int32 := true
	fits_in_uint32 := true
	// Duplicate logic from Khronos's generator.py: use int32_t if everything fits into int32_t,
	// then uint32_t, then int64_t.
	basetype := cpp_types.Int32TType
	for _, element := range enum_types[typе.Name] {
		value, err := enumFieldValue(element, enum_values)
		if err != nil {
			return nil, err
		}
		if int64(int32(value)) != value {
			fits_in_int32 = false
		}
		if int64(uint32(value)) != value {
			fits_in_uint32 = false
		}
	}
	if !fits_in_int32 {
		if fits_in_uint32 {
			basetype = cpp_types.UInt32TType
		} else {
			basetype = cpp_types.Int64TType
		}
	}
	values := []cpp_types.EnumFieldInfo{}
	for _, element := range enum_types[typе.Name] {
		value, _ := enumFieldValue(element, enum_values)
		values = append(values, cpp_types.EnumField(element.Name, basetype, element.Alias, value))
	}
	return cpp_types.EnumType(typе.Name, basetype, values), nil
}

func vulkanFuncPoiterTypeFromXML(typе *typeInfo, types map[string]cpp_types.Type) (cpp_types.Type, error) {
	definition := strings.TrimSpace(typе.RawXML)
	if !strings.HasPrefix(definition, "typedef ") ||
		!strings.HasSuffix(definition, ");") ||
		strings.Count(definition, " (VKAPI_PTR *<name>") != 1 ||
		strings.Count(definition, "</name>)(") != 1 {
		return nil, errors.New("Couldn't determine function type from \"" + definition + "\"")
	}
	split := strings.Split(definition[8:len(definition)-2], " (VKAPI_PTR *<name>")
	var return_type cpp_types.Type
	return_type_string := split[0]
	if strings.HasSuffix(return_type_string, "*") {
		return_typе, ok := types[return_type_string[0:len(return_type_string)-1]]
		if !ok {
			return nil, errors.New("Couldn't determine function type \"" + return_type_string + "\"")
		}
		return_type = cpp_types.PointerType(return_typе)
	} else {
		return_typе, ok := types[return_type_string]
		if !ok {
			return nil, errors.New("Couldn't determine function type \"" + return_type_string + "\"")
		}
		return_type = return_typе
	}
	parameters := strings.Split(split[1], "</name>)(")[1]
	var parameter_types []cpp_types.FieldInfo
	if parameters == "void" {
		return cpp_types.PointerType(cpp_types.FunctionType(return_type, parameter_types)), nil
	}
	for _, parameter := range strings.Split(parameters, ",") {
		parameter = strings.TrimSpace(parameter)
		parameter_type_len := strings.LastIndex(parameter, " ")
		parameter_type := strings.TrimSpace(parameter[:parameter_type_len])
		parameter_name := strings.TrimSpace(parameter[parameter_type_len+1:])
		if strings.HasPrefix(parameter_type, "<type>") &&
			strings.HasSuffix(parameter_type, "</type>") {
			parameter_types = append(
				parameter_types,
				ExtendedField(parameter_name, types[parameter_type[6:len(parameter_type)-7]],
					nil,
					nil))
		} else if strings.HasPrefix(parameter_type, "const <type>") &&
			strings.HasSuffix(parameter_type, "</type>*") {
			pointee_type_name := parameter_type[12 : len(parameter_type)-8]
			if pointee_type, ok := types[pointee_type_name]; ok {
				parameter_types = append(
					parameter_types,
					ExtendedField(
						parameter_name,
						cpp_types.PointerType(cpp_types.ConstType(pointee_type)),
						nil,
						nil))
			} else {
				parameter_types = append(
					parameter_types,
					ExtendedField(
						parameter_name,
						cpp_types.PointerType(cpp_types.ConstType(cpp_types.OpaqueType(pointee_type_name))),
						nil,
						nil))
			}
		} else if strings.HasPrefix(parameter_type, "<type>") &&
			strings.HasSuffix(parameter_type, "</type>*") {
			pointee_type_name := parameter_type[6 : len(parameter_type)-8]
			if pointee_type, ok := types[pointee_type_name]; ok {
				parameter_types = append(
					parameter_types,
					ExtendedField(
						parameter_name,
						cpp_types.PointerType(pointee_type),
						nil,
						nil))
			} else {
				parameter_types = append(
					parameter_types,
					ExtendedField(
						parameter_name,
						cpp_types.PointerType(cpp_types.OpaqueType(pointee_type_name)),
						nil,
						nil))
			}
		} else {
			return nil, errors.New("Couldn't determine parameter type \"" + parameter_type + "\"")
		}
	}
	return cpp_types.PointerType(cpp_types.FunctionType(return_type, parameter_types)), nil
}

func vulkanHandleTypeFromXML(typе *typeInfo, types map[string]cpp_types.Type) (cpp_types.Type, error) {
	if typе.RawXML == fmt.Sprintf("<type>VK_DEFINE_HANDLE</type>(<name>%s</name>)", typе.Name) {
		return cpp_types.AliasType(typе.Name, cpp_types.PointerType(cpp_types.OpaqueType(fmt.Sprintf("struct %s_T", typе.Name)))), nil
	} else if typе.RawXML == fmt.Sprintf("<type>VK_DEFINE_NON_DISPATCHABLE_HANDLE</type>(<name>%s</name>)", typе.Name) {
		return cpp_types.ArchDependentType(
			cpp_types.AliasType(typе.Name, cpp_types.UInt64TType),
			cpp_types.AliasType(typе.Name, cpp_types.PointerType(cpp_types.OpaqueType(fmt.Sprintf("struct %s_T", typе.Name)))),
			cpp_types.AliasType(typе.Name, cpp_types.UInt64TType),
			cpp_types.AliasType(typе.Name, cpp_types.PointerType(cpp_types.OpaqueType(fmt.Sprintf("struct %s_T", typе.Name)))),
			cpp_types.AliasType(typе.Name, cpp_types.UInt64TType),
			cpp_types.AliasType(typе.Name, cpp_types.PointerType(cpp_types.OpaqueType(fmt.Sprintf("struct %s_T", typе.Name))))), nil
	}
	return nil, errors.New("Unexpected handle \"" + typе.Name + "\": \"" + typе.RawXML + "\"\"")
}

func vulkanStructTypeFromXML(typе *typeInfo, optional_struct bool, types map[string]cpp_types.Type, enum_values map[string]*enumFieldInfo) (cpp_types.Type, error) {
	fields_info, err := vulkanStructuralTypeMembersFromXML(typе.Name, typе.Members, types, enum_values)
	if err != nil {
		return nil, err
	}
	optional_enum_value := ""
	if optional_struct {
		if typе.Members[0].Type != "VkStructureType" || fields_info[0].Name() != "sType" || fields_info[1].Name() != "pNext" {
			return nil, errors.New("Struct extension must have first field named VkStructureType sType and second named pNext")
		}
		optional_enum_value = typе.Members[0].Value
	}
	return ExtendedStruct(cpp_types.StructType(typе.Name, fields_info), optional_struct, optional_enum_value), nil
}

func vulkanUnionTypeFromXML(typе *typeInfo, types map[string]cpp_types.Type, enum_values map[string]*enumFieldInfo) (cpp_types.Type, error) {
	fields_info, err := vulkanStructuralTypeMembersFromXML(typе.Name, typе.Members, types, enum_values)
	if err != nil {
		return nil, err
	}
	return cpp_types.UnionType(typе.Name, fields_info), nil
}

var space = regexp.MustCompile(`\s+`)

func vulkanStructuralTypeMembersFromXML(name string, members []structuralMemberInfo, types map[string]cpp_types.Type, enum_values map[string]*enumFieldInfo) (result []cpp_types.FieldInfo, err error) {
	fields_info := []*extendedField{}
	field_map := make(map[string]*extendedField)
	for _, member := range members {
		html := strings.TrimSpace(member.RawXML)
		// Note: checks below count only opening tags because XML parser guarantees that closing tags are there and they
		// match opening tags.
		if member.Comment != "" {
			if comments := strings.Count(html, "<comment>"); comments > 1 {
				return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
			} else if comments == 1 {
				html = strings.Split(html, "<comment>")[0] + strings.Split(html, "</comment>")[1]
			}
		}
		if strings.Count(html, "<type>") != 1 {
			return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
		}
		text_before_type_name := strings.TrimSpace(strings.Split(html, "<type>")[0])
		text_after_type_name := strings.TrimSpace(strings.Split(html, "</type>")[1])
		if strings.Count(html, "<name>") > 1 {
			return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
		} else if strings.Count(html, "<name>") == 1 {
			text_after_type_name = strings.Split(text_after_type_name, "<name>")[0] + strings.Split(text_after_type_name, "</name>")[1]
		}
		text_after_type_name = strings.TrimSpace(space.ReplaceAllString(text_after_type_name, " "))
		member_type, raw_type_known := types[member.Type]
		// TODO(b/268638193): handle comma-separated list of allowed functions.
		if member.Type == "VkBaseInStructure" || member.Type == "VkBaseOutStructure" {
			member_type = types[member.Validstructs]
		}
		if len(text_after_type_name) > 0 && text_after_type_name[0] == '*' {
			if raw_type_known {
				if text_before_type_name == "const" || text_before_type_name == "const struct" {
					member_type = cpp_types.ConstType(member_type)
				} else if text_before_type_name != "" && text_before_type_name != "struct" {
					return nil, errors.New("Unexpected prefix in \"" + name + "\": \"" + html + "\"\"")
				}
			} else {
				member_type = cpp_types.OpaqueType(member.Type)
				// Note that if type is opaque in C (but not C++!) if has to be prefixed with either "const struct" or  "struct".
				// If we only see "const" or nothing then that type is not opaque and is supposed to be declared somewhere below.
				// Return unknownType if that happens.
				if text_before_type_name == "" || text_before_type_name == "const" {
					return nil, unknownType
				} else if text_before_type_name == "const struct" {
					member_type = cpp_types.ConstType(member_type)
				} else if text_before_type_name != "struct" {
					return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
				}
			}
			if text_after_type_name == "*" {
				member_type = cpp_types.PointerType(member_type)
			} else if text_after_type_name == "**" {
				member_type = cpp_types.PointerType(cpp_types.PointerType(member_type))
			} else if text_after_type_name == "* const*" || text_after_type_name == "* const *" {
				member_type = cpp_types.PointerType(cpp_types.ConstType(cpp_types.PointerType(member_type)))
			} else {
				return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
			}
		} else {
			if !raw_type_known {
				return nil, unknownType
			}
			if text_before_type_name == "const" {
				member_type = cpp_types.ConstType(member_type)
			} else if text_before_type_name != "" {
				return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
			}
			// Bitfields are not actually supposed to be used in vk.xml — and it even has comment which says exactly that!
			// Unfortunately they are already there and couldn't be removed (backward compatibility!).
			// Replace "uint32_t :8" with "uint8_t" and "uint32_t :24" with "uint8_t[3]".
			// This is hack but provides proper layout.
			if text_after_type_name == ":8" {
				if member.Type != "uint32_t" && member.Type != "VkGeometryInstanceFlagsKHR" {
					return nil, errors.New("Unsupported bitfield type name \"" + name + "\": \"" + html + "\"\"")
				}
				member_type = cpp_types.UInt8TType
			} else if text_after_type_name == ":24" {
				if member.Type != "uint32_t" {
					return nil, errors.New("Unsupported bitfield type name \"" + name + "\": \"" + html + "\"\"")
				}
				member_type = cpp_types.ArrayType(cpp_types.UInt8TType, 3)
			} else {
				indexes := []uint{}
				for strings.HasSuffix(text_after_type_name, "]") {
					array_size_text := text_after_type_name[strings.LastIndex(text_after_type_name, "[")+1 : len(text_after_type_name)-1]
					text_after_type_name = text_after_type_name[0 : len(text_after_type_name)-len(array_size_text)-2]
					if strings.HasPrefix(array_size_text, "<enum>") {
						if !strings.HasSuffix(array_size_text, "</enum>") {
							return nil, errors.New("Unsupported array index \"" + array_size_text + "\"\"")
						}
						array_size_text = enum_values[array_size_text[6:len(array_size_text)-7]].Value
					}
					array_size, err := strconv.ParseInt(array_size_text, 10, 32)
					if err != nil {
						return nil, err
					}
					indexes = append(indexes, uint(array_size))
				}
				for i := len(indexes) - 1; i >= 0; i-- {
					member_type = cpp_types.ArrayType(member_type, indexes[i])
				}
				if text_after_type_name != "" {
					return nil, errors.New("Unexpected member definition in \"" + name + "\": \"" + html + "\"\"")
				}
			}
		}
		new_field := extendedField{cpp_types.Field(member.Name, member_type), nil, nil}
		if member.Api != "vulkansc" {
			fields_info = append(fields_info, &new_field)
			field_map[member.Name] = &new_field
		}
	}
	for _, member := range members {
		// This strange notion is used in VkAccelerationStructureBuildGeometryInfoKHR structure where only one of two fields can be non-NULL:
		//   <member len="geometryCount,1" optional="true,false">
		// We treat it as <member len="geometryCount" optional="true"> here.
		if strings.HasSuffix(member.Length, ",1") {
			if length, ok := field_map[member.Length[0:len(member.Length)-2]]; ok {
				field_map[member.Name].length = length
			} else {
				return nil, errors.New("Unexpected len field in \"" + member.Name + "\"")
			}
			// Some corner cases have len like “pAllocateInfo->descriptorSetCount”.
			// Currently that only have one level, structures are always input to function,
			// and we don't need to convert these.
			//
			// We need to detect case where that wouldn't be true in the future.
			// Only then would we know how to handle these.
			//
			// We parse these and pass the information to calling module because it's
			// not easy to see here whether types are compatible on all platforms here
			// or not (and there are more than a couple of such types).
		} else if strings.Contains(member.Length, "->") {
			split_length := strings.Split(member.Length, "->")
			if len(split_length) > 2 {
				return nil, errors.New("Unexpected len field in \"" + member.Name + "\"")
			}
			length, ok := field_map[split_length[0]]
			if !ok {
				return nil, errors.New("Unexpected len field in \"" + member.Name + "\"")
			}
			field_map[member.Name].length = length
			// Note: we are dealing with pointer to const data structure here.
			// That's why we dereference twice.
			length_type := length.Type()
			if length_type.Kind(cpp_types.FirstArch) != cpp_types.Ptr {
				return nil, errors.New("Unexpected len field in \"" + member.Name + "\"")
			}
			length_type = length_type.Elem(cpp_types.FirstArch)
			if length_type.Kind(cpp_types.FirstArch) == cpp_types.Const {
				length_type = length_type.Elem(cpp_types.FirstArch)
			}
			if length_type.Kind(cpp_types.FirstArch) != cpp_types.Struct &&
				length_type.Kind(cpp_types.FirstArch) != cpp_types.Union {
				return nil, errors.New("Unexpected len field in \"" + member.Name + "\"")
			}
			for i := uint(0); i < length_type.NumField(cpp_types.FirstArch); i++ {
				if length_type.Field(i, cpp_types.FirstArch).Name() == split_length[1] {
					field_map[member.Name].nested_field = length_type.Field(i, cpp_types.FirstArch)
				}
			}
			if field_map[member.Name].nested_field == nil {
				return nil, errors.New("Unexpected field referred by len in \"" + member.Name + "\"")
			}
			// If len is too complicated it may be represented as LaTeX expression (e.g.
			// latexmath:[\lceil{\mathit{rasterizationSamples} \over 32}\rceil] for pSampleMask)
			// In these cases altlength represents for C, but it may be quite hard to parse that
			// too.
			// Thankfully for now all such complex fields pass arrays of uint{8,16,32}_t which
			// we never translate. Note: we currently don't translate uint64_t even if these
			// are not 100% compatible on all platforms. The only direction where that may
			// matter would be x86 (32bit) to AArch32 translation (which we don't support),
			// but arrays of uint{8,16,32}_t are 100% compatible on all platforums.
			//
			// Verify that it's so and ignore "len" in that case.
		} else if member.AltLength != "" {
			typе := field_map[member.Name].Type()
			if typе.Kind(cpp_types.FirstArch) != cpp_types.Ptr {
				return nil, errors.New("Unexpected altlen field in \"" + member.Name + "\"")
			}
			element_type := typе.Elem(cpp_types.FirstArch)
			if element_type.Kind(cpp_types.FirstArch) == cpp_types.Const {
				element_type = element_type.Elem(cpp_types.FirstArch)
			}
			if element_type.Kind(cpp_types.FirstArch) == cpp_types.Alias {
				element_type = element_type.Elem(cpp_types.FirstArch)
			}
			if element_type.Kind(cpp_types.FirstArch) != cpp_types.UInt8T &&
				element_type.Kind(cpp_types.FirstArch) != cpp_types.UInt16T &&
				element_type.Kind(cpp_types.FirstArch) != cpp_types.UInt32T {
				return nil, errors.New("Unexpected altlen field in \"" + member.Name + "\"")
			}
		} else if member.Length != "" && member.Length != "null-terminated" && !strings.HasSuffix(member.Length, ",null-terminated") {
			if length, ok := field_map[member.Length]; ok {
				field_map[member.Name].length = length
			} else {
				return nil, errors.New("Unexpected len field in \"" + member.Name + "\"")
			}
		}
	}
	result = make([]cpp_types.FieldInfo, len(fields_info))
	for index, field_info := range fields_info {
		result[index] = field_info
	}
	return result, nil
}

var unknownType = errors.New("Couldn't find type")

func elementFromRawXML(element_name string, raw_XML string) (string, error) {
	opening_tag := "<" + element_name + ">"
	closing_tag := "</" + element_name + ">"
	if strings.Count(raw_XML, opening_tag) != 1 {
		return "", errors.New("Couldn't determine element \"" + element_name + "\" from \"" + raw_XML + "\"")
	}
	if strings.Count(raw_XML, closing_tag) != 1 {
		return "", errors.New("Couldn't determine element \"" + element_name + "\" from \"" + raw_XML + "\"")
	}
	return strings.Split(strings.Split(
		raw_XML, opening_tag)[1], closing_tag)[0], nil
}

func parseEnumValues(registry *registry) (map[string]*enumFieldInfo, map[string][]*enumFieldInfo, error) {
	enum_values := make(map[string]*enumFieldInfo)
	enum_types := make(map[string][]*enumFieldInfo)

	for enum_idx := range registry.Enums {
		enum := &registry.Enums[enum_idx]
		for enum_field_idx := range enum.EnumFields {
			enum_field := &enum.EnumFields[enum_field_idx]
			if _, ok := enum_values[enum_field.Name]; ok {
				return nil, nil, errors.New("Duplicated enum value \"" + enum.Name + "\"")
			}
			enum_values[enum_field.Name] = enum_field
			if value, ok := enum_types[enum.Name]; ok {
				enum_types[enum.Name] = append(value, enum_field)
			} else {
				enum_types[enum.Name] = append([]*enumFieldInfo{}, enum_field)
			}
		}
	}
	for feature_idx := range registry.Features {
		feature := &registry.Features[feature_idx]
		for enum_field_idx := range feature.EnumFields {
			enum_field := &feature.EnumFields[enum_field_idx]
			if enum_field.Extends != "" {
				if _, ok := enum_values[enum_field.Name]; ok {
					return nil, nil, errors.New("Duplicated enum value \"" + enum_field.Name + "\"")
				}
				enum_values[enum_field.Name] = enum_field
				enum_types[enum_field.Extends] = append(enum_types[enum_field.Extends], enum_field)
			}
		}
	}
	for extension_idx := range registry.Extensions {
		extension := &registry.Extensions[extension_idx]
		for requires_idx := range extension.Requires {
			requires := &extension.Requires[requires_idx]
			for enum_field_idx := range requires.EnumFields {
				enum_field := &requires.EnumFields[enum_field_idx]
				if enum_field.ExtID == 0 {
					enum_field.ExtID = extension.ID
				}
				if enum_field.Extends != "" {
					if old_enum_filed, ok := enum_values[enum_field.Name]; ok {
						// Some values are declared twice, once as feature and once as extension.
						// It's Ok as long as values match.
						if enum_field.Alias != "" && enum_field.Alias == old_enum_filed.Alias {
							continue
						}
						if enum_field.Alias == "" || old_enum_filed.Alias == "" {
							continue
						}
						value, err1 := enumFieldValue(enum_field, nil)
						old_value, err2 := enumFieldValue(old_enum_filed, nil)
						if value == old_value && err1 == nil && err2 == nil {
							continue
						}
						return nil, nil, errors.New("Duplicated enum value \"" + enum_field.Name + "\"")
					}
					enum_values[enum_field.Name] = enum_field
					enum_types[enum_field.Extends] = append(enum_types[enum_field.Extends], enum_field)
				}
			}
		}
	}
	return enum_values, enum_types, nil
}

func enumFieldValue(enum_field *enumFieldInfo, all_enum_fields map[string]*enumFieldInfo) (int64, error) {
	if enum_field.Value != "" {
		if strings.HasPrefix(enum_field.Value, "0x") {
			return strconv.ParseInt(enum_field.Value[2:], 16, 64)
		}
		return strconv.ParseInt(enum_field.Value, 10, 64)
	}
	if enum_field.BitPos != "" {
		result, err := strconv.ParseInt(enum_field.BitPos, 10, 64)
		if err != nil {
			return 0, err
		}
		return 1 << result, nil
	}
	if enum_field.Alias != "" {
		return enumFieldValue(all_enum_fields[enum_field.Alias], all_enum_fields)
	}
	var result = 1000000000 + (enum_field.ExtID-1)*1000 + enum_field.Offset
	if enum_field.Dir == "" {
		return result, nil
	}
	return -result, nil
}
