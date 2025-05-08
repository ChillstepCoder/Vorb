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
        constexpr f32 AVG_LERP_TIME = 0.15f;
        const ui64 sleepElapsedNS = mSleepTimeElapsed.time_since_epoch().count();
        const ui64 frameTimeNS = totalElapsedNS - sleepElapsedNS;

        // Soft average so it doesn't jump around so much
        const f32 prevFrameTimeMS = mCurrentFrameTimeMS;
        mCurrentFrameTimeMS = util::lerp(prevFrameTimeMS, f32((f64)frameTimeNS / NS_PER_MS), AVG_LERP_TIME);

        mThreadUtilizationPercentRollingAverage[mRollingAverageIndex] = f32v2(sleepElapsedNS / NS_PER_MS, totalElapsedNS / NS_PER_MS);
    }
    else {
        mThreadUtilizationPercentRollingAverage[mRollingAverageIndex] = f32v2(0.0f);
    }

    // Rolling average percentages
    f32v2 rollingTotal(0.0f);
    for (i32 i = 0; i < ROLLING_AVERAGE_SIZE; ++i) {
        rollingTotal += mThreadUtilizationPercentRollingAverage[i];
    }
    if (rollingTotal.y > 0.0f) {

        const f32 totalActiveTime = rollingTotal.y - rollingTotal.x;
        const f32 percent = totalActiveTime / rollingTotal.y;


        //if (IS_SIM_THREAD() && percent >= 0.99f) {
        //    LOG_INFO("SIMTHREAD TIMING {} {} {} {}", rollingTotal.y, rollingTotal.x, totalActiveTime, percent * 100.0f);
        //}


        mCurrentUtilizationPercentage = percent * 100.0f;
        assert(mCurrentUtilizationPercentage <= 100.0f);
    }

    mRollingAverageIndex = (mRollingAverageIndex + 1) % ROLLING_AVERAGE_SIZE;
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
