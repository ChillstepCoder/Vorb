#include "stdafx.h"
#include "TimestepManager.h"

#include <yojimbo/yojimbo.h>

void TimestepManager::init(f64 fixedTimeStepSec) {
    mTimeStepSec = fixedTimeStepSec;
    mTimeSec = yojimbo_time();
}

bool TimestepManager::tryTick(f64* sleepSec) {
    const f64 currentTime = yojimbo_time();
    // Fixed ticks
    const f64 timeLoad = mTimeSec.load();
    if (timeLoad <= currentTime) {
        mTimeSec = timeLoad + mTimeStepSec;
        return true;
    }
    *sleepSec = timeLoad - currentTime;
    return false;
}
