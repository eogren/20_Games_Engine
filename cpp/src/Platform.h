#pragma once

namespace platform
{
    /**
     * Monotonic seconds-since-boot. Must share a time base with
     * CAMetalDisplayLinkUpdate.targetPresentationTimestamp so the two can be
     * subtracted directly (currently both back onto CACurrentMediaTime).
     */
    double currentTime();
} // namespace platform