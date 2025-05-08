#pragma once

#include <chrono>

class ThreadUtilizationTimer
{
public:
    ThreadUtilizationTimer();

    void beginFrame();
    // Call before calling Sleep()
    void beginSleep();
    // Call after calling Sleep()
    void endSleep();

    f32 getUtilizationPercentage() const { return mCurrentUtilizationPercentage; }
    f32 getFrameTimeMS() const { return mCurrentFrameTimeMS; }
private:

    std::chrono::high_resolution_clock::time_point mFrameBegin;
    std::chrono::high_resolution_clock::time_point mSleepBegin;
    std::chrono::high_resolution_clock::time_point mSleepTimeElapsed;

    std::atomic<f32> mCurrentUtilizationPercentage = 0.0f;
    std::atomic<f32> mCurrentFrameTimeMS = 0;

    inline static constexpr i32 ROLLING_AVERAGE_SIZE = 16;
    f32v2 mThreadUtilizationPercentRollingAverage[ROLLING_AVERAGE_SIZE] = {};
    i32 mRollingAverageIndex = 0;

    bool mCapturingSleep = false;
    bool mFrameBegan = false;
};

