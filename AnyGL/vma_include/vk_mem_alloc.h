// Outer compatibility shim. The vendor VMA checkout is read-only.
#pragma once
#undef VK_MAKE_VERSION
#define VK_MAKE_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))
// Keep the vendor version definition intact. ANGLE's VMA wrapper uses it to
// provide the Vulkan 1.1+ properties2 function introduced in VMA 3.4.
#include "../angle/third_party/vulkan_memory_allocator/include/vk_mem_alloc.h"
