#pragma once

// Time manager that implements fixed timestep for updates
class GameTimeManager
{
public:
    void init(f64 fixedTimeStepSec);

    bool tryTick(f64* sleepSec);
    TimeStampSec getCurrentTimeSec() const { return mTimeSec; }
    f64 getTimestep() const { return mTimeStepSec; }

private:
    f64 mTimeStepSec = 0.0; // Target timestep
    TimeStampSec mTimeSec = 0.0; // Current tracked time
};

