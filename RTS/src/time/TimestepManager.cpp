#include "stdafx.h"
#include "TimestepManager.h"

#include <yojimbo/yojimbo.h>

void TimestepManager::init(f64 fixedTimeStepSec, int maxFramesAhead /*= INT32_MAX*/) {
    mTimeStepSec = fixedTimeStepSec;
    mNextTickTime = yojimbo_time();
    mMaxFramesAhead = maxFramesAhead;
}

bool TimestepManager::tryTick(f64* sleepSec) {
    const f64 currentTime = yojimbo_time();
    // Fixed ticks
    const f64 tickTime = mNextTickTime.load();
    if (tickTime <= currentTime) {
        if (tickTime < currentTime - mTimeStepSec * mMaxFramesAhead) [[unlikely]] {
            // Skip ahead
            mNextTickTime = currentTime + mTimeStepSec;
        }
        else {
            mNextTickTime = tickTime + mTimeStepSec;
        }
        return true;
    }
    *sleepSec = tickTime - currentTime;
    return false;
}
