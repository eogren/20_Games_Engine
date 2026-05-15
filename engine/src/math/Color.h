#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace engine
{

    // Linear-space RGBA color in [0,1] per channel. Storage is RGB-internal;
    // HSV is a construction-and-manipulation convenience. The HSV path makes
    // single-axis effects natural — "brighter over time" is a bump on V, "hue
    // drift" is a bump on H — where the same effects in RGB would require
    // coordinated multi-channel scaling that desaturates toward white and
    // drifts hue if applied unevenly.
    class Color
    {
    public:
        constexpr Color() = default;

        // Construct from RGB(A) components in [0,1]. Out-of-range values are
        // kept verbatim — clamping is a swapchain-format concern, not a math-
        // type concern.
        static constexpr Color rgb(float r, float g, float b, float a = 1.0f)
        {
            return Color{r, g, b, a};
        }

        // Construct from HSV(A) where all three axes are normalized to [0,1).
        // Hue wraps modulo 1.0, so 0.0 and 1.0 are both red. Uniform [0,1)
        // ranges across h/s/v match the renderer's [0,1] RGB convention; if
        // a degree-based API ever feels more natural, add a `Color::hsv_deg`
        // helper rather than re-typing the canonical one.
        static Color hsv(float h, float s, float v, float a = 1.0f);

        static constexpr Color black()
        {
            return Color{0.0f, 0.0f, 0.0f, 1.0f};
        }
        static constexpr Color white()
        {
            return Color{1.0f, 1.0f, 1.0f, 1.0f};
        }

        constexpr float r() const
        {
            return r_;
        }
        constexpr float g() const
        {
            return g_;
        }
        constexpr float b() const
        {
            return b_;
        }
        constexpr float a() const
        {
            return a_;
        }

        // HSV view, computed on demand from the stored RGB. Greys (max == min)
        // collapse hue to 0 by convention; saturation is 0 in that case so the
        // hue value is operationally irrelevant. Roundtrip through hsv()
        // -> {hue,saturation,value} is exact up to float precision.
        float hue() const;
        float saturation() const;
        float value() const;

        // Return a copy with one HSV axis replaced. Path is RGB -> HSV ->
        // swap-axis -> RGB, so each call is a small handful of float ops —
        // fine for per-frame use, not for tight inner loops. Call site stays
        // readable: c.with_value(c.value() + 0.1f * dt).
        Color with_hue(float h) const;
        Color with_saturation(float s) const;
        Color with_value(float v) const;
        constexpr Color with_alpha(float new_a) const
        {
            return Color{r_, g_, b_, new_a};
        }

        // Raw {r,g,b,a} view for APIs that want a contiguous float4 (e.g. the
        // Vulkan VkClearColorValue.float32 array).
        constexpr std::array<float, 4> floats() const
        {
            return {r_, g_, b_, a_};
        }

    private:
        constexpr Color(float r, float g, float b, float a) : r_{r}, g_{g}, b_{b}, a_{a} {}

        float r_ = 0.0f;
        float g_ = 0.0f;
        float b_ = 0.0f;
        float a_ = 1.0f;
    };

    inline Color Color::hsv(float h, float s, float v, float a)
    {
        h = h - std::floor(h); // wrap into [0,1)
        const float c = v * s;
        const float h6 = h * 6.0f;
        const float x = c * (1.0f - std::fabs(std::fmod(h6, 2.0f) - 1.0f));
        const float m = v - c;

        float r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;
        if (h6 < 1.0f)
        {
            r1 = c;
            g1 = x;
        }
        else if (h6 < 2.0f)
        {
            r1 = x;
            g1 = c;
        }
        else if (h6 < 3.0f)
        {
            g1 = c;
            b1 = x;
        }
        else if (h6 < 4.0f)
        {
            g1 = x;
            b1 = c;
        }
        else if (h6 < 5.0f)
        {
            r1 = x;
            b1 = c;
        }
        else
        {
            r1 = c;
            b1 = x;
        }

        return Color::rgb(r1 + m, g1 + m, b1 + m, a);
    }

    inline float Color::hue() const
    {
        const float max = std::max({r_, g_, b_});
        const float min = std::min({r_, g_, b_});
        const float delta = max - min;
        if (delta == 0.0f) return 0.0f;

        float h = 0.0f;
        if (max == r_)
            h = std::fmod((g_ - b_) / delta, 6.0f);
        else if (max == g_)
            h = (b_ - r_) / delta + 2.0f;
        else
            h = (r_ - g_) / delta + 4.0f;

        h /= 6.0f;
        if (h < 0.0f) h += 1.0f;
        return h;
    }

    inline float Color::saturation() const
    {
        const float max = std::max({r_, g_, b_});
        if (max == 0.0f) return 0.0f;
        const float min = std::min({r_, g_, b_});
        return (max - min) / max;
    }

    inline float Color::value() const
    {
        return std::max({r_, g_, b_});
    }

    inline Color Color::with_hue(float h) const
    {
        return Color::hsv(h, saturation(), value(), a_);
    }

    inline Color Color::with_saturation(float s) const
    {
        return Color::hsv(hue(), s, value(), a_);
    }

    inline Color Color::with_value(float v) const
    {
        return Color::hsv(hue(), saturation(), v, a_);
    }

} // namespace engine
