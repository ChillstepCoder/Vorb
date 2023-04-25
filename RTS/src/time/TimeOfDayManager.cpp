#include "stdafx.h"
#include "TimeOfDayManager.h"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

#include "options/DebugOptions.h"

void TimeOfDayManager::setTimeOfDay(f32 timeHours) {
    assert(timeHours >= 0.0f && timeHours <= HOURS_PER_DAY);

    // Offset debug time
    mTimeOfDayHours = timeHours;
}

void TimeOfDayManager::updateTimeOfDay(f32 timePassedHours) {
    const float SUNRISE_TIME = 6.0f; // 6am
    const float SUN_HEIGHT_OFFSET = 0.3f; // Smaller exponent means brighter days

    mTimeOfDayHours += timePassedHours;
    if (mTimeOfDayHours > HOURS_PER_DAY) {
        mTimeOfDayHours -= HOURS_PER_DAY;
    }

    const f32 sunDelta = (mTimeOfDayHours - SUNRISE_TIME) / 24.0f;
    const f32 sunRotate = sunDelta * M_PIF * 2.0f;
    mSunPosition = glm::rotateY(f32v3(-1.0f, 0.0f, 0.0f), sunRotate);
    mSunHeight = glm::min(mSunPosition.z + SUN_HEIGHT_OFFSET, 0.999f); // Store sun height before modification, cap at an epsilon to fix sampler issue
    mSunPosition.z += 0.2f; // Make it more up lol
    mSunPosition = glm::normalize(mSunPosition);

    mSkyRotMatrix = glm::rotate(sunRotate, f32v3(0.0f, 1.0f, 0.0f));

    // Colors TODO: DIFFERENT
    const f32v3& sunSet = sDebugOptions.mSunColorSunset;
    const f32v3& sunPeak = sDebugOptions.mSunColorPeak;
    const float c = vmath::max(mSunHeight, 0.0f);
    mSunColor = f32v3(
        vmath::lerp(sunSet.r, sunPeak.r, c),
        vmath::lerp(sunSet.g, sunPeak.g, c),
        vmath::lerp(sunSet.b, sunPeak.b, c)
    );

}
