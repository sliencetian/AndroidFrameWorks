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

#define VK_ENABLE_BETA_EXTENSIONS 1
#include <vulkan/vk_layer_interface.h>
#include <vulkan/vulkan.h>

#include <map>
#include <mutex>
#include <tuple>
#include <utility>

#include "berberis/base/logging.h"
#include "berberis/base/strings.h"
#include "berberis/guest_abi/function_wrappers.h"
#include "berberis/guest_abi/guest_arguments.h"
#include "berberis/guest_abi/guest_params.h"
#include "berberis/guest_loader/guest_loader.h"
#include "berberis/proxy_loader/proxy_library_builder.h"
#include "berberis/runtime_primitives/known_guest_function_wrapper.h"
#include "berberis/runtime_primitives/runtime_library.h"

#include "binary_search.h"
#include "vulkan_xml.h"

namespace berberis {

namespace {

std::mutex g_primary_command_buffer_mutex;
// Map from VkCommandBuffer opaque handle to bool value which is true when command buffer is primary.
// Note: we have to handle primary and secondary command buffers differently in vkBeginCommandBuffer,
// but that function, itself, doesn't have any means to know that.
std::map<VkCommandBuffer, bool> g_primary_command_buffer;

void DoCustomTrampolineWithThunk_vkAllocateCommandBuffers(HostCode callee, ProcessState* state) {
  PFN_vkAllocateCommandBuffers callee_function = AsFuncPtr(callee);
  auto [device_guest, pAllocateInfo_guest, pCommandBuffers_guest] =
      GuestParamsValues<PFN_vkAllocateCommandBuffers>(state);
  [[maybe_unused]] bool out_of_memory;
  VkDevice device_host = device_guest;
  GuestType<const struct VkCommandBufferAllocateInfo*>::HostHolder pAllocateInfo_holder;
  const struct VkCommandBufferAllocateInfo* pAllocateInfo_host =
      ToHostType(pAllocateInfo_guest, pAllocateInfo_holder, out_of_memory);
  VkCommandBuffer* pCommandBuffers_host = pCommandBuffers_guest;
  auto&& [ret] = GuestReturnReference<PFN_vkAllocateCommandBuffers>(state);
  ret = callee_function(device_host, pAllocateInfo_host, pCommandBuffers_host);
  if (ret >= VkResult::BERBERIS_VK_SUCCESS) {
    std::lock_guard lock(g_primary_command_buffer_mutex);
    for (uint32_t idx = 0; idx < pAllocateInfo_host->commandBufferCount; ++idx) {
      VkCommandBuffer command_buffer = pCommandBuffers_host[idx];
      // We may be called with the same set of command buffers because of layers.
      if (g_primary_command_buffer.find(command_buffer) != g_primary_command_buffer.end()) {
        continue;
      }
      g_primary_command_buffer.insert(
          std::pair{command_buffer,
                    pAllocateInfo_host->level ==
                        VkCommandBufferLevel::BERBERIS_VK_COMMAND_BUFFER_LEVEL_PRIMARY});
    }
  }
}

void DoCustomTrampolineWithThunk_vkBeginCommandBuffer(HostCode callee, ProcessState* state) {
  PFN_vkBeginCommandBuffer callee_function = AsFuncPtr(callee);
  auto [commandBuffer_guest, pBeginInfo_guest] = GuestParamsValues<PFN_vkBeginCommandBuffer>(state);
  bool out_of_memory;
  VkCommandBuffer commandBuffer_host = commandBuffer_guest;
  bool convert_inheritance_info = false;
  {
    std::lock_guard lock(g_primary_command_buffer_mutex);
    if (auto it = g_primary_command_buffer.find(commandBuffer_guest);
        it != g_primary_command_buffer.end()) {
      convert_inheritance_info = !it->second;
    }
  }
  GuestType<const struct VkCommandBufferBeginInfo*>::HostHolder pBeginInfo_holder;
  const struct VkCommandBufferBeginInfo* pBeginInfo_host =
      ToHostType(pBeginInfo_guest, pBeginInfo_holder, convert_inheritance_info, out_of_memory);
  auto&& [ret] = GuestReturnReference<PFN_vkBeginCommandBuffer>(state);
  ret = callee_function(commandBuffer_host, pBeginInfo_host);
}

void DoCustomTrampolineWithThunk_vkFreeCommandBuffers(HostCode callee, ProcessState* state) {
  PFN_vkFreeCommandBuffers callee_function = AsFuncPtr(callee);
  auto [device_guest, commandPool_guest, commandBufferCount_guest, pCommandBuffers_guest] =
      GuestParamsValues<PFN_vkFreeCommandBuffers>(state);
  VkDevice device_host = device_guest;
  VkCommandPool commandPool_host = commandPool_guest;
  std::uint32_t commandBufferCount_host = commandBufferCount_guest;
  const VkCommandBuffer* pCommandBuffers_host = pCommandBuffers_guest;
  {
    std::lock_guard lock(g_primary_command_buffer_mutex);
    for (uint32_t idx = 0; idx < commandBufferCount_host; ++idx) {
      VkCommandBuffer command_buffer = pCommandBuffers_host[idx];
      // We may be called with the same set of command buffers because of layers.
      if (auto it = g_primary_command_buffer.find(command_buffer);
          it != g_primary_command_buffer.end()) {
        g_primary_command_buffer.erase(it);
      }
    }
  }
  callee_function(device_host, commandPool_host, commandBufferCount_host, pCommandBuffers_host);
}

template <typename VkResultType>
void FilterOutExtensionProperties(VkResultType& result,
                                  uint32_t* properties_out_buf_size,
                                  VkExtensionProperties* properties_out_buf,
                                  uint32_t properties_in_buf_size,
                                  VkExtensionProperties* properties_in_buf) {
  const auto& extensions_map = GetExtensionsMap();
  uint32_t property_count = 0;
  for (uint32_t i = 0; i < properties_in_buf_size; ++i) {
    if (auto conversion = FindElementByName(extensions_map, properties_in_buf[i].extensionName)) {
      if (!properties_out_buf) {
        property_count++;
        continue;
      }
      if (property_count == *properties_out_buf_size) {
        result = VkResult::BERBERIS_VK_INCOMPLETE;
        return;
      }
      properties_out_buf[property_count++] = properties_in_buf[i];
      // Some extensions get new revisions over time and since we don't know if they may introduce
      // new functions we reduce version the latest known to us.
      if (properties_out_buf[property_count - 1].specVersion > conversion->maxsupported_spec) {
        properties_out_buf[property_count - 1].specVersion = conversion->maxsupported_spec;
      }
    }
  }
  *properties_out_buf_size = property_count;
}

void DoCustomTrampolineWithThunk_vkEnumerateDeviceExtensionProperties(HostCode callee,
                                                                      ProcessState* state) {
  PFN_vkEnumerateDeviceExtensionProperties callee_function = AsFuncPtr(callee);
  auto [physicalDevice_guest, pLayerName_guest, pPropertyCount_guest, pProperties_guest] =
      GuestParamsValues<PFN_vkEnumerateDeviceExtensionProperties>(state);
  VkPhysicalDevice physicalDevice_host = physicalDevice_guest;
  const char* pLayerName_host = pLayerName_guest;
  std::uint32_t* pPropertyCount_host = pPropertyCount_guest;
  struct VkExtensionProperties* pProperties_host = pProperties_guest;
  // This function is called twice with nullptr to get the buffer size and with the buffer itself to
  // get the extensions. Technically the number of extensions may change between these two calls, so
  // it should be valid to return unfiltered buffer size on the first call, and then apply the
  // filter only on the second call and return a smaller buffer size in addition to the buffer
  // itself. But CTS verifies that the size doesn't change between the calls. Thus we need to do the
  // filtering even on the first call to figure out the size after the filtering.
  //
  // Technically consistent results are not guaranteed, but since official Vulkan dEQP tests rely on
  // that particularity it should be possible to achieve it in practice.
  auto&& [ret] = GuestReturnReference<PFN_vkEnumerateDeviceExtensionProperties>(state);
  for (;;) {
    uint32_t properties_in_buf_size;
    ret = callee_function(physicalDevice_host, pLayerName_host, &properties_in_buf_size, nullptr);
    if (ret < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }

    uint32_t properties_in_buf_size2 = properties_in_buf_size;
    VkExtensionProperties properties_in_buf[properties_in_buf_size];
    ret = callee_function(
        physicalDevice_host, pLayerName_host, &properties_in_buf_size2, properties_in_buf);
    if (ret < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }
    if (properties_in_buf_size != properties_in_buf_size2 ||
        ret == VkResult::BERBERIS_VK_INCOMPLETE) {
      continue;
    }

    FilterOutExtensionProperties(
        ret, pPropertyCount_host, pProperties_host, properties_in_buf_size, properties_in_buf);
    return;
  }
}

void DoCustomTrampolineWithThunk_vkEnumerateInstanceExtensionProperties(HostCode callee,
                                                                        ProcessState* state) {
  PFN_vkEnumerateInstanceExtensionProperties callee_function = AsFuncPtr(callee);
  auto [pLayerName_guest, pPropertyCount_guest, pProperties_guest] =
      GuestParamsValues<PFN_vkEnumerateInstanceExtensionProperties>(state);
  const char* pLayerName_host = pLayerName_guest;
  std::uint32_t* pPropertyCount_host = pPropertyCount_guest;
  struct VkExtensionProperties* pProperties_host = pProperties_guest;
  auto&& [ret] = GuestReturnReference<PFN_vkEnumerateInstanceExtensionProperties>(state);
  for (;;) {
    uint32_t properties_in_buf_size;
    ret = callee_function(pLayerName_host, &properties_in_buf_size, nullptr);
    if (ret < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }

    uint32_t properties_in_buf_size2 = properties_in_buf_size;
    VkExtensionProperties properties_in_buf[properties_in_buf_size];
    ret = callee_function(pLayerName_host, &properties_in_buf_size2, properties_in_buf);
    if (ret < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }
    if (properties_in_buf_size != properties_in_buf_size2 ||
        ret == VkResult::BERBERIS_VK_INCOMPLETE) {
      continue;
    }

    FilterOutExtensionProperties(
        ret, pPropertyCount_host, pProperties_host, properties_in_buf_size, properties_in_buf);
    return;
  }
}

void DoCustomTrampolineWithThunk_vkGetDeviceProcAddr(HostCode callee, ProcessState* state) {
  PFN_vkGetDeviceProcAddr callee_function = AsFuncPtr(callee);
  auto [device, function_name] = GuestParamsValues<PFN_vkGetDeviceProcAddr>(state);
  const auto& function_map = GetMapForvkGetProcAddr();
  if (auto conversion = FindElementByName(function_map, function_name)) {
    auto func = callee_function(device, function_name);
    WrapHostFunctionImpl(reinterpret_cast<void*>(func), conversion->trampoline, function_name);
    auto&& [ret] = GuestReturnReference<PFN_vkGetDeviceProcAddr>(state);
    ret = PFN_vkVoidFunction(func);
    return;
  }
  ALOGE("Unknown function is used with vkGetDeviceProcAddr: %s",
        static_cast<const char*>(function_name));
  auto&& [ret] = GuestReturnReference<PFN_vkGetDeviceProcAddr>(state);
  ret = PFN_vkVoidFunction(0);
}

void DoCustomTrampolineWithThunk_vkGetInstanceProcAddr(HostCode callee, ProcessState* state) {
  PFN_vkGetInstanceProcAddr callee_function = AsFuncPtr(callee);
  auto [instance, function_name] = GuestParamsValues<PFN_vkGetInstanceProcAddr>(state);
  const auto& function_map = GetMapForvkGetProcAddr();
  if (auto conversion = FindElementByName(function_map, function_name)) {
    auto func = callee_function(instance, function_name);
    WrapHostFunctionImpl(reinterpret_cast<void*>(func), conversion->trampoline, function_name);
    auto&& [ret] = GuestReturnReference<PFN_vkGetInstanceProcAddr>(state);
    ret = PFN_vkVoidFunction(func);
    return;
  }
  ALOGE("Unknown function is used with vkGetInstanceProcAddr: %s",
        static_cast<const char*>(function_name));
  auto&& [ret] = GuestReturnReference<PFN_vkGetInstanceProcAddr>(state);
  ret = PFN_vkVoidFunction(0);
}

void RunGuest_vkEnumerateDeviceExtensionProperties(GuestAddr pc, GuestArgumentBuffer* buf) {
  auto [physicalDevice_host, pLayerName_host, pPropertyCount_host, pProperties_host] =
      HostArgumentsValues<PFN_vkEnumerateDeviceExtensionProperties>(buf);
  for (;;) {
    uint32_t properties_in_buf_size;
    auto [physicalDevice_guest, pLayerName_guest, pPropertyCount_guest, pProperties_guest] =
        GuestArgumentsReferences<PFN_vkEnumerateDeviceExtensionProperties>(buf);
    physicalDevice_guest = physicalDevice_host;
    pLayerName_guest = pLayerName_host;
    pPropertyCount_guest = &properties_in_buf_size;
    pProperties_guest = nullptr;
    RunGuestCall(pc, buf);
    auto&& [result] = HostResultReference<PFN_vkEnumerateDeviceExtensionProperties>(buf);
    if (result < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }

    uint32_t properties_in_buf_size2 = properties_in_buf_size;
    VkExtensionProperties properties_in_buf[properties_in_buf_size];
    physicalDevice_guest = physicalDevice_host;
    pLayerName_guest = pLayerName_host;
    pPropertyCount_guest = &properties_in_buf_size;
    pProperties_guest = properties_in_buf;
    RunGuestCall(pc, buf);
    if (result < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }
    if (properties_in_buf_size != properties_in_buf_size2 ||
        result == VkResult::BERBERIS_VK_INCOMPLETE) {
      continue;
    }

    FilterOutExtensionProperties(
        result, pPropertyCount_host, pProperties_host, properties_in_buf_size, properties_in_buf);
    return;
  }
}

void RunGuest_vkEnumerateInstanceExtensionProperties(GuestAddr pc, GuestArgumentBuffer* buf) {
  auto [pLayerName_host, pPropertyCount_host, pProperties_host] =
      HostArgumentsValues<PFN_vkEnumerateInstanceExtensionProperties>(buf);
  for (;;) {
    uint32_t properties_in_buf_size;
    auto [pLayerName_guest, pPropertyCount_guest, pProperties_guest] =
        GuestArgumentsReferences<PFN_vkEnumerateInstanceExtensionProperties>(buf);
    pLayerName_guest = pLayerName_host;
    pPropertyCount_guest = &properties_in_buf_size;
    pProperties_guest = nullptr;
    RunGuestCall(pc, buf);
    auto&& [result] = HostResultReference<PFN_vkEnumerateInstanceExtensionProperties>(buf);
    if (result < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }

    uint32_t properties_in_buf_size2 = properties_in_buf_size;
    VkExtensionProperties properties_in_buf[properties_in_buf_size];
    pLayerName_guest = pLayerName_host;
    pPropertyCount_guest = &properties_in_buf_size;
    pProperties_guest = properties_in_buf;
    RunGuestCall(pc, buf);
    if (result < VkResult::BERBERIS_VK_SUCCESS) {
      return;
    }
    if (properties_in_buf_size != properties_in_buf_size2 ||
        result == VkResult::BERBERIS_VK_INCOMPLETE) {
      continue;
    }

    FilterOutExtensionProperties(
        result, pPropertyCount_host, pProperties_host, properties_in_buf_size, properties_in_buf);
    return;
  }
}

void RunGuest_vkCreateInstance(GuestAddr pc, GuestArgumentBuffer* buf) {
  auto [pCreateInfo_host, pAllocator_host, pInstance_host] =
      HostArgumentsValues<PFN_vkCreateInstance>(buf);
  {
    [[maybe_unused]] bool out_of_memory;
    auto [pCreateInfo_guest, pAllocator_guest, pInstance_guest] =
        GuestArgumentsReferences<PFN_vkCreateInstance>(buf);

    const VkLayerInstanceCreateInfo* layer_create_info =
        bit_cast<const VkLayerInstanceCreateInfo*>(pCreateInfo_host);

    // Step through the pNext chain until we get to the link function
    while (layer_create_info &&
           (layer_create_info->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
            layer_create_info->function != VK_LAYER_FUNCTION_LINK)) {
      layer_create_info = static_cast<const VkLayerInstanceCreateInfo*>(layer_create_info->pNext);
    }
    if (layer_create_info) {
      void* func = bit_cast<void*>(layer_create_info->u.pLayerInfo->pfnNextGetInstanceProcAddr);
      WrapHostFunctionImpl(
          func, DoCustomTrampolineWithThunk_vkGetInstanceProcAddr, "NextGetInstanceProcAddr");
    }

    GuestType<const struct VkAllocationCallbacks*>::GuestHolder pAllocator_holder;
    pAllocator_guest =
        GuestType<const struct VkAllocationCallbacks*>(pAllocator_host, pAllocator_holder, out_of_memory);
    RunGuestCall(pc, buf);
  }
}

void RunGuest_vkGetDeviceProcAddr(GuestAddr pc, GuestArgumentBuffer* buf) {
  const auto& function_map = GetMapForRunGuestvkGetInstanceProcAddr();

  auto [device, function_name] = HostArgumentsValues<PFN_vkGetDeviceProcAddr>(buf);

  if (auto conversion = FindElementByName(function_map, function_name)) {
    RunGuestCall(pc, buf);
    auto&& [host_result] = HostResultReference<PFN_vkGetDeviceProcAddr>(buf);
    auto [guest_result] = GuestResultValue<PFN_vkGetDeviceProcAddr>(buf);
    host_result = bit_cast<PFN_vkVoidFunction>(conversion->wrapper(ToGuestAddr(guest_result)));
    return;
  }
  ALOGE("Unknown function is used with vkGetDeviceProcAddr: %s", function_name);
  auto&& [result] = HostResultReference<PFN_vkGetDeviceProcAddr>(buf);
  result = bit_cast<PFN_vkVoidFunction>(nullptr);
}

void RunGuest_vkGetInstanceProcAddr(GuestAddr pc, GuestArgumentBuffer* buf) {
  const auto& function_map = GetMapForRunGuestvkGetInstanceProcAddr();

  auto [instance, function_name] = HostArgumentsValues<PFN_vkGetInstanceProcAddr>(buf);

  if (auto conversion = FindElementByName(function_map, function_name)) {
    RunGuestCall(pc, buf);
    auto&& [host_result] = HostResultReference<PFN_vkGetDeviceProcAddr>(buf);
    auto [guest_result] = GuestResultValue<PFN_vkGetDeviceProcAddr>(buf);
    host_result = bit_cast<PFN_vkVoidFunction>(conversion->wrapper(ToGuestAddr(guest_result)));
    return;
  }
  ALOGE("Unknown function is used with vkGetInstanceProcAddr: %s", function_name);
  auto&& [result] = HostResultReference<PFN_vkGetDeviceProcAddr>(buf);
  result = bit_cast<PFN_vkVoidFunction>(nullptr);
}

}  // namespace

#if defined(NATIVE_BRIDGE_GUEST_ARCH_ARM) && defined(__i386__)

#include "trampolines_arm_to_x86-inl.h"  // generated file NOLINT [build/include]

#elif defined(NATIVE_BRIDGE_GUEST_ARCH_ARM64) && defined(__x86_64__)

#include "trampolines_arm64_to_x86_64-inl.h"  // generated file NOLINT [build/include]

#elif defined(NATIVE_BRIDGE_GUEST_ARCH_RISCV64) && defined(__x86_64__)

#include "trampolines_riscv64_to_x86_64-inl.h"  // generated file NOLINT [build/include]

#else

#error "Unknown guest/host arch combination"

#endif

extern "C" void InitProxyLibrary(ProxyLibraryBuilder* builder) {
  builder->Build("libvulkan.so",
                 sizeof(kKnownTrampolines) / sizeof(kKnownTrampolines[0]),
                 kKnownTrampolines,
                 sizeof(kKnownVariables) / sizeof(kKnownVariables[0]),
                 kKnownVariables);
  for (const auto& named_wrapper : GetMapForRunGuestvkGetInstanceProcAddr()) {
    RegisterKnownGuestFunctionWrapper(named_wrapper.name, named_wrapper.wrapper);
  }
}

}  // namespace berberis
