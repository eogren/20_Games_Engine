#include "platform/surface.h"
#include "platform_impl.h"

#include <vulkan/vulkan_win32.h>

namespace platform
{
    std::span<const char* const> requiredInstanceExtensions()
    {
        // Names are #define'd as string literals by vulkan_core.h and
        // vulkan_win32.h; the array is plain platform-neutral data once
        // preprocessing finishes, so the header doesn't have to pull in
        // vulkan_win32.h.
        static constexpr const char* kExts[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        };
        return kExts;
    }

    std::expected<VkSurfaceKHR, VkResult> createSurface(VkInstance instance, const Platform& platform)
    {
        const auto& impl = PlatformInternals::get(platform);

        VkWin32SurfaceCreateInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = ::GetModuleHandle(nullptr),
            .hwnd = impl.wnd,
        };

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        const VkResult r = vkCreateWin32SurfaceKHR(instance, &info, nullptr, &surface);
        if (r != VK_SUCCESS)
        {
            return std::unexpected(r);
        }
        return surface;
    }

    VkExtent2D framebufferExtent(const Platform& platform)
    {
        const auto& impl = PlatformInternals::get(platform);
        RECT rect{};
        // GetClientRect returns the client area (drawable region), which is
        // what Vulkan wants for surface/swapchain sizing — not the outer
        // window rect including title bar and borders.
        ::GetClientRect(impl.wnd, &rect);
        return VkExtent2D{
            .width = static_cast<uint32_t>(rect.right - rect.left),
            .height = static_cast<uint32_t>(rect.bottom - rect.top),
        };
    }

} // namespace platform
