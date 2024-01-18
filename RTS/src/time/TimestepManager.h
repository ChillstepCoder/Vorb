#pragma once

// Time manager that implements fixed timestep for updates
class TimestepManager
{
public:
    void init(f64 fixedTimeStepSec);

    bool tryTick(f64* sleepSec);
    TimeStampSec getCurrentTimeSec() const { return mTimeSec; }
    f64 getTimestepSec() const { return mTimeStepSec; }
    void setTargetTimestepSec(f64 timestepSec) { mTimeStepSec = timestepSec; }

private:
    f64 mTimeStepSec = 0.0; // Target timestep
    std::atomic<TimeStampSec> mTimeSec = 0.0; // Current tracked time
};

