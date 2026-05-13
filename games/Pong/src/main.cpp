// Pong — entry point.
//
// At scaffold time this just proves that engine + platform link into an
// executable. Win32 window creation + Vulkan instance + game loop all
// land in follow-up commits.

#include "engine.h"
#include "platform.h"

#include <cstdio>

int main() {
    std::printf("Pong - engine %s, platform %s\n",
                engine::version(), platform::version());
    return 0;
}
