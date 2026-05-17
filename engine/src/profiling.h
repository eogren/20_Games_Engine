#pragma once

#ifdef ENGINE_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#else
// Stubs for when Tracy is disabled (ENGINE_ENABLE_TRACY=OFF). Tracy.hpp
// defines these same empty macros when TRACY_ENABLE is unset, but the
// header itself is only available in the `tracy` build preset.
#define FrameMark
#define ZoneScoped
#define ZoneScopedN(x)
#endif
