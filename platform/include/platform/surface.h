#pragma once

#include <volk.h>

#include <expected>
#include <span>

namespace platform
{
    class Platform;

    /**
     * Instance-level extensions this backend needs in order to produce a
     * VkSurfaceKHR. Returned span points at static storage; safe to keep
     * indefinitely. Engine queries this when building the VkInstance so
     * games don't have to know they're running on Win32 vs anything else.
     */
    std::span<const char* const> requiredInstanceExtensions();

    /**
     * Build a Vulkan surface for the given platform's window.
     *
     * The implementation lives in the per-backend TU (src/win32/surface_win32.cpp
     * today) and is the *only* place in the codebase where Vulkan meets the
     * native windowing API.
     */
    std::expected<VkSurfaceKHR, VkResult> createSurface(VkInstance instance, const Platform& platform);

    /**
     * Current size of the platform window's client area, in pixels.
     *
     * Paired with createSurface: callers feed this straight into swapchain
     * creation. Returns VkExtent2D rather than a portable struct so the
     * value is usable verbatim at the Vulkan boundary; lives in surface.h
     * (not platform.h) to keep the Vulkan include cost off TUs that only
     * touch Platform's OS surface.
     *
     * Returns {0, 0} for a minimized window — caller is expected to skip
     * swapchain (re)creation in that state rather than treat it as an error.
     */
    VkExtent2D framebufferExtent(const Platform& platform);

} // namespace platform
