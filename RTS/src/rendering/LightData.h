#pragma once

enum class LightShape {
    Point,
    Count
};

enum class LightAttenuationType {
    Linear,
    Count
};

class LightData
{
public:
    ui8v3 mColor = ui8v3(255);
    f32 mIntensity = 1.0f;
    f32 mInnerRadiusCoef = 0.01f; // < 1.0f
    f32 mOuterRadius = 10.0f;
    LightShape mLightShape = LightShape::Point;
    LightAttenuationType mAttenuationShape = LightAttenuationType::Linear;
};

