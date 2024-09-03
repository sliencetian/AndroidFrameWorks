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

#ifndef VULKAN_XML_H_
#define VULKAN_XML_H_

#include <unistd.h>

#include <cstdint>

#include "berberis/base/bit_util.h"
#include "berberis/base/struct_check.h"
#ifndef BERBERIS_LAYOUT_CHECK_ONLY
#include "berberis/guest_abi/function_wrappers.h"
#include "berberis/guest_abi/guest_arguments.h"
#include "berberis/guest_abi/guest_function_wrapper_signature.h"
#include "berberis/guest_abi/guest_params.h"
#include "berberis/guest_abi/guest_type.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/guest_function_wrapper_impl.h"
#include "berberis/runtime_primitives/host_code.h"
#include "berberis/runtime_primitives/host_function_wrapper_impl.h"
#include "berberis/runtime_primitives/runtime_library.h"

#include "binary_search.h"
#endif

// Note: we only need these defines at the end of xvulkan_xml-inl.h and would like to not include it
// prematurely but vulkan_xml_define.h couldn't be included from vulkan_xml-inl.h when these two
// files are in different filegroups.
#include "vulkan_xml_define.h"

// These are explicitly written here as a workaround.  These are platform types that are missing
// from vulkan_types.  They are implicitly declared in the berberis namespace because the Vulkan
// structures that include them use the "struct X* y;" field syntax.  However, they belong in the
// global namespace to avoid conflicts in files that include the host headers for these types,
// which are included via vulkan/vulkan.h.
struct AHardwareBuffer;
struct ANativeWindow;
struct Display;
struct wl_display;
struct xcb_connection_t;

namespace berberis {

#ifndef BERBERIS_LAYOUT_CHECK_ONLY

template <typename ResultType, typename... ArgumentType>
inline ResultType (*WrapGuestFunctionIfNeeded(GuestType<ResultType (*)(ArgumentType...)> func,
                                              const char* name))(ArgumentType...) {
  auto host_func =
      reinterpret_cast<ResultType (*)(ArgumentType...)>(UnwrapHostFunction(ToGuestAddr(func)));
  if (host_func) {
    return host_func;
  }
  return WrapGuestFunction(func, name);
}

template <GuestAbi::CallingConventionsVariant kCallingConventionsVariant = GuestAbi::kDefaultAbi,
          typename Func>
inline GuestType<Func> WrapHostFunctionIfNeeded(Func func, const char* name) {
  auto host_func = UnwrapHostFunction(ToGuestAddr(func));
  if (!host_func) {
    WrapHostFunction(func, name);
  }
  return func;
}

class GuestHolderBase {
 public:
  GuestHolderBase() = default;
  virtual ~GuestHolderBase() = default;
};

class HostHolderBase {
 public:
  HostHolderBase() = default;
  virtual ~HostHolderBase() = default;
};

// Note: anonymous namespace is subtly different from static: each anonymous namespace has internal
// unique name, but is imported into encompassing namespace.
//
// It's unclear whether it's valid to declare function in one such namespace and then define in the
// other one: MSVC says it's impossible, other compilers allow that.
//
// But even compilers which support such use are confused when friend delaration is used.
//
// Use of “static” works without any surprises: https://godbolt.org/z/sczb7r5Gn
static const void* ConvertOptionalStructures(GuestType<const void*> head,
                                             std::unique_ptr<HostHolderBase>& holder,
                                             bool& out_of_memory);
static void* ConvertOptionalStructures(GuestType<void*> head,
                                       std::unique_ptr<HostHolderBase>& holder,
                                       bool& out_of_memory);
static GuestType<const void*> ConvertOptionalStructures(const void* head,
                                                        std::unique_ptr<GuestHolderBase>& holder,
                                                        bool& out_of_memory);
static GuestType<void*> ConvertOptionalStructures(void* head,
                                                  std::unique_ptr<GuestHolderBase>& holder,
                                                  bool& out_of_memory);
#endif  // BERBERIS_LAYOUT_CHECK_ONLY

namespace {

#define BERBERIS_VK_DEFINE_HANDLE(name) using name = struct name##_T*

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || \
    defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) ||       \
    defined(__powerpc64__)
#define BERBERIS_VK_DEFINE_NON_DISPATCHABLE_HANDLE(name) \
  BERBERIS_VK_DEFINE_HANDLE(name)
#else
#define BERBERIS_VK_DEFINE_NON_DISPATCHABLE_HANDLE(name) using name = std::uint64_t
#endif

#if defined(_WIN32)
    // On Windows, Vulkan commands use the stdcall convention
    #define BERBERIS_VKAPI_PTR  __stdcall
#elif defined(__ANDROID__) && defined(__ARM_ARCH) && __ARM_ARCH < 7
    // 32bit Android ARMv6 or earlier don't support Vulkan by design.
    #error "Vulkan isn't supported for the 'armeabi' NDK ABI"
#elif defined(__ANDROID__) && defined(__ARM_ARCH) && __ARM_ARCH >= 7 && defined(__ARM_32BIT_STATE)
    // 32bit Android ARMv7+ are using aapcs-vfp.
    #define BERBERIS_VKAPI_PTR  __attribute__((pcs("aapcs-vfp")))
#else
    // On other platforms, use the default calling convention
    #define BERBERIS_VKAPI_PTR
#endif

// API Constants.
// TODO(232598137): Parse them from XML instead.
constexpr uint32_t BERBERIS_VK_TRUE = 1;
constexpr uint32_t BERBERIS_VK_FALSE = 0;

#include "vulkan_xml-inl.h"  // generated file NOLINT [build/include]

#endif  // VULKAN_XML_H_
