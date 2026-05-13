#pragma once

#include <compare>
#include <numbers>

namespace engine
{

    // Unit-tagged angle. Construct with Angle::from_radians / Angle::from_degrees
    // (or the _rad / _deg literals) so the unit is explicit at every call site
    // that takes an angle. Bare-float parameters silently assume radians, which
    // is the classic source of "why is my FOV 57° instead of 1 radian?" bugs.
    //
    // Storage is radians-canonical; degrees are computed.
    class Angle
    {
    public:
        constexpr Angle() = default;

        static constexpr Angle from_radians(float v)
        {
            return Angle{v};
        }
        static constexpr Angle from_degrees(float v)
        {
            return Angle{v * (std::numbers::pi_v<float> / 180.0f)};
        }
        static constexpr Angle zero()
        {
            return Angle{0.0f};
        }

        constexpr float radians() const
        {
            return radians_;
        }
        constexpr float degrees() const
        {
            return radians_ * (180.0f / std::numbers::pi_v<float>);
        }

        constexpr auto operator<=>(const Angle&) const = default;

        constexpr Angle operator-() const
        {
            return Angle{-radians_};
        }
        constexpr Angle operator+(Angle o) const
        {
            return Angle{radians_ + o.radians_};
        }
        constexpr Angle operator-(Angle o) const
        {
            return Angle{radians_ - o.radians_};
        }
        constexpr Angle operator*(float s) const
        {
            return Angle{radians_ * s};
        }
        constexpr Angle operator/(float s) const
        {
            return Angle{radians_ / s};
        }

        constexpr Angle& operator+=(Angle o)
        {
            radians_ += o.radians_;
            return *this;
        }
        constexpr Angle& operator-=(Angle o)
        {
            radians_ -= o.radians_;
            return *this;
        }
        constexpr Angle& operator*=(float s)
        {
            radians_ *= s;
            return *this;
        }
        constexpr Angle& operator/=(float s)
        {
            radians_ /= s;
            return *this;
        }

    private:
        explicit constexpr Angle(float radians) : radians_{radians} {}
        float radians_ = 0.0f;
    };

    constexpr Angle operator*(float s, Angle a)
    {
        return a * s;
    }

    // Opt-in literals: `using namespace engine::math_literals;` then `45.0_deg` or `1.57_rad`.
    namespace math_literals
    {
        constexpr Angle operator""_rad(long double v)
        {
            return Angle::from_radians(static_cast<float>(v));
        }
        constexpr Angle operator""_deg(long double v)
        {
            return Angle::from_degrees(static_cast<float>(v));
        }
    } // namespace math_literals

} // namespace engine
