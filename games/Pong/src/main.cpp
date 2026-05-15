// Pong — entry point.

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

    // Dark navy — placeholder for game-controlled clear color until a real
    // Camera/scene API supersedes this stateful setter on the renderer.
    eng.renderer().setClearColor(0.05f, 0.08f, 0.18f);

    eng.run();
    return 0;
}
