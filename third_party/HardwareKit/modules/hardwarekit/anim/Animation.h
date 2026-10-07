#pragma once

#include <cmath>
#include <algorithm>

/*  HardwareKit control motion. Every change to a control, whoever makes it, is animated by the same
    critically damped follower, so the visual path is identical regardless of source.  is an
    opaque tag the caller can use to tint the control (who moved it).
*/
namespace hwk::anim
{
    /** Exponential approach, frame-rate independent. */
    inline float approach (float current, float target, float ratePerSecond, float dt) noexcept
    {
        return target + (current - target) * std::exp (-ratePerSecond * dt);
    }

    struct KnobAnimator
    {
        float angle = 0.0f, velocity = 0.0f;
        float activity = 0.0f;   // 1 while moving, then decays
        float hover = 0.0f;
        bool initialised = false;

        int boundParam = -1;
        juce::uint32 lastCounter = 0;
        int source = 0;

        static constexpr float omega = 55.0f; // rad/s, settles in ~0.1 s

        void update (float targetAngle, bool changed, int changeSource, bool hovered, float dt) noexcept
        {
            if (! initialised)
            {
                angle = targetAngle;
                velocity = 0.0f;
                initialised = true;
            }

            // Exact critically-damped spring step (stable for any dt)
            const float x = angle - targetAngle;
            const float e = std::exp (-omega * dt);
            const float k = velocity + omega * x;
            angle    = targetAngle + (x + k * dt) * e;
            velocity = (velocity - omega * k * dt) * e;

            if (changed)
            {
                activity = 1.0f;
                source = changeSource;
            }
            else if (! isSettled (targetAngle))
            {
                activity = std::max (activity, 0.85f);
            }
            else
            {
                activity = std::max (0.0f, activity - dt / 1.4f);
            }

            hover = approach (hover, hovered ? 1.0f : 0.0f, 14.0f, dt);
        }

        bool isSettled (float targetAngle) const noexcept
        {
            return std::abs (angle - targetAngle) < 5.0e-4f && std::abs (velocity) < 5.0e-3f;
        }

        bool isIdle (float targetAngle, bool hovered) const noexcept
        {
            return isSettled (targetAngle) && activity <= 0.0f && std::abs (hover - (hovered ? 1.0f : 0.0f)) < 1.0e-3f;
        }
    };

    /** Latching push button: the cap dips when the value flips (and stays down while held),
        the LED fades between states. */
    struct ButtonAnimator
    {
        bool state = false, initialised = false;
        float press = 0.0f, led = 0.0f;

        static constexpr float release = 0.14f;   // seconds for the cap to come back up

        void update (bool target, bool held, float dt) noexcept
        {
            if (! initialised)
            {
                state = target;
                led = target ? 1.0f : 0.0f;
                initialised = true;
            }

            if (target != state)
            {
                state = target;
                press = 1.0f;
            }

            press = held ? 1.0f : std::max (0.0f, press - dt / release);
            led = approach (led, target ? 1.0f : 0.0f, 22.0f, dt);
        }

        /** 0..1 cap travel with a soft return. */
        float travel() const noexcept { return press * press * (3.0f - 2.0f * press); }

        bool isIdle() const noexcept { return press <= 0.0f && std::abs (led - (state ? 1.0f : 0.0f)) < 1.0e-3f; }
    };

    /** Bat toggle lever: quick snap with slight overshoot (~6%). */
    struct ToggleAnimator
    {
        bool state = false, initialised = false;
        float from = 0.0f, to = 0.0f, t = 1.0f, angle = 0.0f;

        static constexpr float duration = 0.08f;

        static float snap (float x) noexcept
        {
            constexpr float c1 = 1.25f, c3 = c1 + 1.0f;
            const float u = x - 1.0f;
            return 1.0f + c3 * u * u * u + c1 * u * u;
        }

        void update (bool target, float onAngle, float offAngle, float dt) noexcept
        {
            const float targetAngle = target ? onAngle : offAngle;

            if (! initialised)
            {
                state = target;
                angle = from = to = targetAngle;
                t = 1.0f;
                initialised = true;
                return;
            }

            if (target != state)
            {
                state = target;
                from = angle;
                to = targetAngle;
                t = 0.0f;
            }

            if (t < 1.0f)
            {
                t = std::min (1.0f, t + dt / duration);
                angle = from + (to - from) * snap (t);
            }
            else
            {
                angle = to;
            }
        }

        bool isIdle() const noexcept { return t >= 1.0f; }
    };
}
