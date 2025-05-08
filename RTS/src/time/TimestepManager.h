#pragma once

// Time manager that implements fixed timestep for updates
class TimestepManager
{
public:
    void init(f64 fixedTimeStepSec, int maxFramesAhead = INT32_MAX);

    bool tryTick(f64* sleepSec);
    TimeStampSec getCurrentTimeSec() const { return mNextTickTime; }
    f64 getTimestepSec() const { return mTimeStepSec; }
    void setTargetTimestepSec(f64 timestepSec) { mTimeStepSec = timestepSec; }

private:
    f64 mTimeStepSec = 0.0; // Target timestep
    std::atomic<TimeStampSec> mNextTickTime = 0.0; // Current tracked time
    int mMaxFramesAhead = 2; // Max frames ahead before forcing catchup
};

