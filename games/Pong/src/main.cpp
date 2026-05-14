// Pong — entry point.
//
// At scaffold time this just proves that engine + platform link into an
// executable. Win32 window creation + Vulkan instance + game loop all
// land in follow-up commits.

#include "engine.h"
#include "log/Log.h"
#include "platform.h"
#include "win32/platform.h"
#include "win32/win32_check.h"

#include <spdlog/spdlog.h>

int main()
{
    engine::log::init({.file_path = "pong.log"});
    spdlog::info("Pong - engine {}, platform {}", engine::version(), platform::version());

    // TODO: source this from platform once platform exposes
    // requiredInstanceExtensions(). For now the game knows it's running on
    // win32, which is fine while platform is still a stub.
    static constexpr const char* kPlatformExtensions[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
    };

    platform::win32::Platform host{"Pong"};
    engine::Engine eng;
    if (auto r = eng.initRenderer("Pong", kPlatformExtensions); !r)
    {
        spdlog::error("Renderer init failed: {}", static_cast<int>(r.error()));
        return 1;
    }

    host.GameLoop();
    return 0;
}
