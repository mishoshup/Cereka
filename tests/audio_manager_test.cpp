#include <gtest/gtest.h>
#include "cereka_audio_manager.hpp"
#include <cmath>

using namespace cereka;

namespace {
float applyCurve(float t, FadeCurve curve)
{
    switch (curve) {
        case FadeCurve::Linear:    return t;
        case FadeCurve::EaseIn:    return t * t;
        case FadeCurve::EaseOut:   return 1.0f - (1.0f - t) * (1.0f - t);
        case FadeCurve::EaseInOut:
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }
    return t;
}
}

TEST(AudioFadeTest, LinearCurveStart)
{
    EXPECT_NEAR(applyCurve(0.0f, FadeCurve::Linear), 0.0f, 0.001f);
}

TEST(AudioFadeTest, LinearCurveEnd)
{
    EXPECT_NEAR(applyCurve(1.0f, FadeCurve::Linear), 1.0f, 0.001f);
}

TEST(AudioFadeTest, LinearCurveMid)
{
    EXPECT_NEAR(applyCurve(0.5f, FadeCurve::Linear), 0.5f, 0.001f);
}

TEST(AudioFadeTest, EaseInCurve)
{
    EXPECT_NEAR(applyCurve(0.5f, FadeCurve::EaseIn), 0.25f, 0.001f);
}

TEST(AudioFadeTest, EaseOutCurve)
{
    EXPECT_NEAR(applyCurve(0.5f, FadeCurve::EaseOut), 0.75f, 0.001f);
}

TEST(AudioFadeTest, EaseInOutCurve)
{
    EXPECT_NEAR(applyCurve(0.25f, FadeCurve::EaseInOut), 0.125f, 0.001f);
    EXPECT_NEAR(applyCurve(0.75f, FadeCurve::EaseInOut), 0.875f, 0.001f);
}

TEST(AudioFadeTest, FadeOutGain)
{
    float t = 0.3f;
    float gain = applyCurve(1.0f - t, FadeCurve::Linear);
    EXPECT_NEAR(gain, 0.7f, 0.001f);
}

TEST(AudioFadeTest, FadeStateDefaults)
{
    BgmFade fade;
    EXPECT_EQ(fade.state, BgmFade::State::None);
    EXPECT_EQ(fade.timer, 0.0f);
    EXPECT_EQ(fade.duration, 0.0f);
}
