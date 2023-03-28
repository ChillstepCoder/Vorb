#include "stdafx.h"
#include "ThreadUtilizationTimer.h"


constexpr f64 NS_PER_MS = 1000000.0;

ThreadUtilizationTimer::ThreadUtilizationTimer() : mFrameBegin(std::chrono::high_resolution_clock::now())
{

}

void ThreadUtilizationTimer::beginFrame() {
    assert(!mCapturingSleep);

    const std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();

    const ui64 totalElapsedNS = (currentTime - mFrameBegin).count();

    if (totalElapsedNS != 0) {
        // TODO: Rolling average is better?
        constexpr f32 AVG_LERP_PERCENT = 0.02f;
        constexpr f32 AVG_LERP_TIME = 0.1f;
        const ui64 sleepElapsedNS = mSleepTimeElapsed.time_since_epoch().count();
        const ui64 frameTimeNS = totalElapsedNS - sleepElapsedNS;
        const f64 percentageTimeSleeping = (f64)sleepElapsedNS / (f64)totalElapsedNS;

        // Soft average so it doesn't jump around so much
        const f32 prevFrameTimeMS = mCurrentFrameTimeMS;
        mCurrentFrameTimeMS = vmath::lerp(prevFrameTimeMS, f32((f64)frameTimeNS / NS_PER_MS), AVG_LERP_TIME);
        const f32 prevUtilization = mCurrentUtilizationPercentage;
        mCurrentUtilizationPercentage = vmath::lerp(prevUtilization, (f32)(100.0 * (1.0 - percentageTimeSleeping)), AVG_LERP_PERCENT);
        assert(mCurrentUtilizationPercentage <= 100.0f);
    }


    mFrameBegin = currentTime;
    mSleepTimeElapsed = std::chrono::high_resolution_clock::time_point();
}

void ThreadUtilizationTimer::beginSleep() {
    assert(!mCapturingSleep);
    mCapturingSleep = true;
    mSleepBegin = std::chrono::high_resolution_clock::now();
}

void ThreadUtilizationTimer::endSleep() {
    assert(mCapturingSleep);
    mCapturingSleep = false;

    const std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
    mSleepTimeElapsed += currentTime - mSleepBegin;
}
