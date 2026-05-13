// Smoke test for engine + doctest wiring. Asserts the scaffold builds
// and links, nothing more. Replaced as real engine surface area lands.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "engine.h"

#include <cstring>

TEST_CASE("engine::version returns a non-empty string")
{
    const char* v = engine::version();
    REQUIRE(v != nullptr);
    REQUIRE(std::strlen(v) > 0);
}
