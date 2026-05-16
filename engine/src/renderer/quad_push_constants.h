#pragma once

// Layout of the push-constant block consumed by engine/shaders/quad.slang.
// Natural-aligned: mat4 at 0, vec4s at 64 and 80. Slang stores matrices
// column-major in cbuffer/push_constants by default and glm stores them the
// same way, so memcpy via vkCmdPushConstants matches the shader's view bit
// for bit.

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace renderer
{
    struct QuadPushConstants
    {
        glm::mat4 viewProj;
        glm::vec4 rect;  // (x, y, w, h) in pixel-space, top-left origin
        glm::vec4 color; // RGBA, linear
    };

    static_assert(sizeof(QuadPushConstants) == 96);
    // Vulkan's spec-mandated minimum for maxPushConstantsSize is 128 bytes.
    static_assert(sizeof(QuadPushConstants) <= 128, "exceeds guaranteed maxPushConstantsSize");
} // namespace renderer
