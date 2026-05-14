// Pong — entry point.
//
// At scaffold time this just proves that engine + platform link into an
// executable. Win32 window creation + Vulkan instance + game loop all
// land in follow-up commits.

#include "engine.h"
#include "log/Log.h"
#include "platform/platform.h"

#include <spdlog/spdlog.h>

int main()
{
    engine::log::init({.file_path = "pong.log"});
    spdlog::info("Pong - engine {}, platform {}", engine::version(), platform::version());

    platform::Platform platform{"Pong"};
    engine::Engine eng{platform};
    if (auto r = eng.initRenderer("Pong"); !r)
    {
        spdlog::error("Renderer init failed: {}", static_cast<int>(r.error()));
        return 1;
    }

    eng.run();
    return 0;
}
