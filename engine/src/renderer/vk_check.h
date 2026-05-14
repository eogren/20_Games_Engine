#pragma once

#include "volk.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <expected>

namespace renderer
{
    inline const char* vkResultString(VkResult result)
    {
        switch (result)
        {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL:
            return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN:
            return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION:
            return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_PIPELINE_COMPILE_REQUIRED:
            return "VK_PIPELINE_COMPILE_REQUIRED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV:
            return "VK_ERROR_INVALID_SHADER_NV";
        default:
            return "VK_UNKNOWN_RESULT";
        }
    }

    [[noreturn]] inline void vkCheckFail(VkResult result, const char* expr, const char* file, int line)
    {
        spdlog::critical("[vulkan] {} failed: {} ({}) at {}:{}", expr, vkResultString(result), static_cast<int>(result),
                         file, line);
        spdlog::default_logger()->flush();
        std::abort();
    }

    inline void vkCheckImpl(VkResult result, const char* expr, const char* file, int line)
    {
        if (result != VK_SUCCESS) vkCheckFail(result, expr, file, line);
    }

    inline void vkTryLog(VkResult result, const char* expr, const char* file, int line)
    {
        spdlog::error("[vulkan] {} returned {} ({}) at {}:{}", expr, vkResultString(result), static_cast<int>(result),
                      file, line);
    }
} // namespace renderer

#define VK_CHECK(expr) ::renderer::vkCheckImpl((expr), #expr, __FILE__, __LINE__)

// Early-return propagator for fallible call sites. Must be used inside a
// function whose return type is std::expected<T, VkResult> (or implicitly
// convertible from std::unexpected<VkResult>). Anything other than
// VK_SUCCESS — including success-like codes such as VK_SUBOPTIMAL_KHR or
// VK_INCOMPLETE — short-circuits as std::unexpected; if you need to
// observe those distinctly, inspect the VkResult directly.
#define VK_TRY(expr)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        if (VkResult _vk_try_result = (expr); _vk_try_result != VK_SUCCESS)                                            \
        {                                                                                                              \
            ::renderer::vkTryLog(_vk_try_result, #expr, __FILE__, __LINE__);                                           \
            return std::unexpected(_vk_try_result);                                                                    \
        }                                                                                                              \
    } while (0)
