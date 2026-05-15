#pragma once

#include <memory>
#include <string_view>

namespace platform
{
    // Backend internals, defined in src/ alongside the active backend's Impl.
    // Forward-declared here so Platform can friend it without leaking native
    // window or Vulkan types into this header.
    struct PlatformInternals;

    /**
     * OS-portable platform service: window, event pump, lifecycle.
     *
     * The concrete backend (Win32 today; iOS later via MoltenVK) lives entirely
     * under src/. This header intentionally pulls neither <windows.h> nor any
     * Vulkan header, so engine TUs that only need the cross-platform interface
     * pay no transitive include cost. Surface creation is a separate free
     * function in <platform/surface.h>.
     */
    class Platform
    {
    public:
        // Forward decl only — full definition lives in src/<backend>/platform_impl.h.
        // Naming Platform::Impl is harmless without the definition; access to impl_
        // still requires going through PlatformInternals (friend).
        struct Impl;

        explicit Platform(std::string_view windowName);
        ~Platform();

        Platform(const Platform&) = delete;
        Platform& operator=(const Platform&) = delete;
        Platform(Platform&&) = delete;
        Platform& operator=(Platform&&) = delete;

        // Reveal the window. Deferred from construction so renderer init
        // (Vulkan instance/device/swapchain — hundreds of ms) doesn't run with
        // the empty client area on screen and produce a flash before the first
        // present. Engine::run() calls this just before entering its loop.
        void show();

        // Drain whatever OS events have queued since the last call. Non-blocking:
        // returns once the message queue is empty. Engine calls this once per
        // frame at the top of its loop (see CLAUDE.md > "Frame ordering").
        void pollEvents();

        // True once the OS has told us to terminate (WM_QUIT on Win32). Engine
        // loops until this flips.
        [[nodiscard]] bool shouldClose() const noexcept;

    private:
        std::unique_ptr<Impl> impl_;

        friend struct PlatformInternals;
    };

    const char* version();

} // namespace platform
