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

package vulkan_types

import (
	"berberis/cpp_types"
	"fmt"
)

var DisplayType = cpp_types.OpaqueType("Display")

var DWORDType = cpp_types.AliasType("DWORD", cpp_types.UInt32TType)

var GgpFrameTokenType = cpp_types.AliasType("GgpFrameToken", cpp_types.UInt64TType)

var GgpStreamDescriptorType = cpp_types.AliasType(
	"GgpStreamDescriptor", cpp_types.UInt32TType)

var HANDLEType = cpp_types.AliasType(
	"HANDLE", cpp_types.PointerType(cpp_types.VoidType))

var HINSTANCEType = cpp_types.AliasType("HINSTANCE", HANDLEType)

var HMONITORType = cpp_types.AliasType("HMONITOR", HANDLEType)

var HWNDType = cpp_types.AliasType("HWND", HANDLEType)

// Note: that type must be converted like similar types in GLES/SLES,
// but we don't have support for these on Android.
var IDirectFBType = cpp_types.AliasType(
	"IDirectFB", cpp_types.OpaqueType("struct IDirectFB"))

// Note: that type must be converted like similar types in GLES/SLES,
// but we don't have support for these on Android.
var IDirectFBSurfaceType = cpp_types.AliasType(
	"IDirectFBSurface", cpp_types.OpaqueType("struct IDirectFBSurface"))

// Note: LPCWSTR is supposed to be pointer to Windows's 16bit const wchar_t string, but on Linux
// (including Android) wchar_t is 32bit type. We're using char16_t for portability: that's not
// canonical definition of LPCWSTR but it's the same on all platforms and on Windows it's compatible
// with canonical one.
var LPCWSTRType = cpp_types.AliasType("LPCWSTR",
	cpp_types.PointerType(cpp_types.ConstType(cpp_types.Char16TType)))

var XIDType = cpp_types.AliasType("XID", cpp_types.ULongType)

var RROutputType = cpp_types.AliasType("RROutput", XIDType)

var SECURITY_ATTRIBUTESType = cpp_types.OpaqueType("SECURITY_ATTRIBUTES")

var VisualIDType = cpp_types.AliasType("VisualID", cpp_types.ULongType)

var WindowType = cpp_types.AliasType("Window", XIDType)

var WLDisplayType = cpp_types.OpaqueType("wl_display")

var WLSurfaceType = cpp_types.OpaqueType("wl_surface")

var XcbConnectionTType = cpp_types.OpaqueType("xcb_connection_t")

var XcbVisualidTType = cpp_types.AliasType("xcb_visualid_t", cpp_types.UInt32TType)

var XcbWindowTType = cpp_types.AliasType("xcb_window_t", cpp_types.UInt32TType)

var ZxHandleTType = cpp_types.AliasType("zx_handle_t", cpp_types.UInt32TType)

func PlatformTypes() map[string]cpp_types.Type {
	return map[string]cpp_types.Type{
		"_screen_context":                      cpp_types.VoidType, // Treat as opaque type for now.
		"_screen_window":                       cpp_types.VoidType, // Treat as opaque type for now.
		"_screen_buffer":                       cpp_types.VoidType, // Treat as opaque type for now.
		"NvSciSyncAttrList":                    cpp_types.IntType,
		"NvSciSyncObj":                         cpp_types.IntType,
		"NvSciSyncFence":                       cpp_types.IntType,
		"NvSciBufAttrList":                     cpp_types.IntType,
		"NvSciBufObj":                          cpp_types.IntType,
		"char":                                 cpp_types.CharType,
		"Display":                              DisplayType,
		"double":                               cpp_types.Float64Type,
		"DWORD":                                DWORDType,
		"float":                                cpp_types.Float32Type,
		"GgpFrameToken":                        GgpFrameTokenType,
		"GgpStreamDescriptor":                  GgpStreamDescriptorType,
		"HANDLE":                               HANDLEType,
		"HINSTANCE":                            HINSTANCEType,
		"HMONITOR":                             HMONITORType,
		"HWND":                                 HWNDType,
		"IDirectFB":                            IDirectFBType,
		"IDirectFBSurface":                     IDirectFBSurfaceType,
		"int":                                  cpp_types.IntType,
		"int8_t":                               cpp_types.Int8TType,
		"int16_t":                              cpp_types.Int16TType,
		"int32_t":                              cpp_types.Int32TType,
		"int64_t":                              cpp_types.Int64TType,
		"LPCWSTR":                              LPCWSTRType,
		"XID":                                  XIDType,
		"RROutput":                             RROutputType,
		"SECURITY_ATTRIBUTES":                  SECURITY_ATTRIBUTESType,
		"size_t":                               cpp_types.SizeTType,
		"StdVideoDecodeH264Mvc":                cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoDecodeH264MvcElement":         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoDecodeH264MvcElementFlags":    cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoDecodeH264PictureInfo":        cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoDecodeH264PictureInfoFlags":   cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoDecodeH264ReferenceInfo":      cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoDecodeH264ReferenceInfoFlags": cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoDecodeH265PictureInfo":        cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoDecodeH265PictureInfoFlags":   cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoDecodeH265ReferenceInfo":      cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoDecodeH265ReferenceInfoFlags": cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH264PictureInfo":        cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264PictureInfoFlags":   cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH264RefListModEntry":    cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264RefMemMgmtCtrlOperations":   cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264RefMgmtFlags":               cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH264RefPicMarkingEntry":         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264ReferenceInfo":              cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264ReferenceListsInfo":         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264ReferenceInfoFlags":         cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH264SliceHeader":                cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH264SliceHeaderFlags":           cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH265PictureInfo":                cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH265PictureInfoFlags":           cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH265ReferenceInfo":              cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH265ReferenceListsInfo":         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH265ReferenceInfoFlags":         cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH265ReferenceModificationFlags": cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH265ReferenceModifications":     cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH265SliceHeader":                cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH265SliceHeaderFlags":           cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoEncodeH265SliceSegmentHeader":         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoEncodeH265SliceSegmentHeaderFlags":    cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH264AspectRatioIdc":                   cpp_types.IntType,
		"StdVideoH264CabacInitIdc":                     cpp_types.IntType,
		"StdVideoH264ChromaFormatIdc":                  cpp_types.IntType,
		"StdVideoH264DisableDeblockingFilterIdc":       cpp_types.IntType,
		"StdVideoH264HrdParameters":                    cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH264Level":                            cpp_types.IntType,
		"StdVideoH264LevelIdc":                         cpp_types.IntType,
		"StdVideoH264MemMgmtControlOp":                 cpp_types.IntType,
		"StdVideoH264ModificationOfPicNumsIdc":         cpp_types.IntType,
		"StdVideoH264PictureParameterSet":              cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH264PictureType":                      cpp_types.IntType,
		"StdVideoH264PocType":                          cpp_types.IntType,
		"StdVideoH264PpsFlags":                         cpp_types.IntType, // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoH264ProfileIdc":                       cpp_types.IntType,
		"StdVideoH264ScalingLists":                     cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH264SequenceParameterSet":             cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH264SequenceParameterSetVui":          cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH264SliceType":                        cpp_types.IntType,
		"StdVideoH264SpsFlags":                         cpp_types.IntType, // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoH264SpsVuiFlags":                      cpp_types.IntType, // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoH264WeightedBiPredIdc":                cpp_types.IntType,
		"StdVideoH264WeightedBipredIdc":                cpp_types.IntType,
		"StdVideoH265PictureParameterSet":              cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265DecPicBufMgr":                     cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265HrdFlags":                         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265HrdParameters":                    cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265Level":                            cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265LevelIdc":                         cpp_types.IntType,
		"StdVideoH265PictureType":                      cpp_types.IntType,
		"StdVideoH265PpsFlags":                         cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265PredictorPaletteEntries":          cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265ProfileIdc":                       cpp_types.IntType,
		"StdVideoH265ScalingLists":                     cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265SequenceParameterSet":             cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265SequenceParameterSetVui":          cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265SliceType":                        cpp_types.IntType,
		"StdVideoH265SpsFlags":                         cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoH265SpsVuiFlags":                      cpp_types.IntType,  // That's actually a struct with bitfields, but it's compatible with int32_t.
		"StdVideoH265SubLayerHrdParameters":            cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265VideoParameterSet":                cpp_types.VoidType, // Treat as opaque type for now.
		"StdVideoH265VpsFlags":                         cpp_types.VoidType, // Treat as opaque type for now.
		"uint8_t":                                      cpp_types.UInt8TType,
		"uint16_t":                                     cpp_types.UInt16TType,
		"uint32_t":                                     cpp_types.UInt32TType,
		"uint64_t":                                     cpp_types.UInt64TType,
		"VisualID":                                     VisualIDType,
		"void":                                         cpp_types.VoidType,
		"Window":                                       WindowType,
		"wl_display":                                   WLDisplayType,
		"wl_surface":                                   WLSurfaceType,
		"xcb_connection_t":                             XcbConnectionTType,
		"xcb_visualid_t":                               XcbVisualidTType,
		"xcb_window_t":                                 XcbWindowTType,
		"zx_handle_t":                                  ZxHandleTType}
}

func IsVulkanHandle(typе cpp_types.Type) bool {
	for arch := cpp_types.FirstArch; arch <= cpp_types.LastArch; arch++ {
		if !isVulkanHandle(typе, arch) {
			return false
		}
	}
	return true
}

func IsVulkanNondispatchableHandle(typе cpp_types.Type) bool {
	return isVulkanNondispatchableHandle(typе, cpp_types.Arm) &&
		isVulkanNondispatchableHandle(typе, cpp_types.X86) &&
		isVulkanHandle(typе, cpp_types.Arm64) &&
		isVulkanHandle(typе, cpp_types.X86_64)
}

func isVulkanHandle(typе cpp_types.Type, arch cpp_types.Arch) bool {
	return typе.Kind(arch) == cpp_types.Alias &&
		typе.Elem(arch).Kind(arch) == cpp_types.Ptr &&
		typе.Elem(arch).Elem(arch).Kind(arch) == cpp_types.Opaque &&
		typе.Elem(arch).Elem(arch).Name(arch) == fmt.Sprintf("struct %s_T", typе.Name(arch))
}

func isVulkanNondispatchableHandle(typе cpp_types.Type, arch cpp_types.Arch) bool {
	return typе.Kind(arch) == cpp_types.Alias &&
		typе.Elem(arch).Kind(arch) == cpp_types.UInt64T
}
