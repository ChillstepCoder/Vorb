#include "stdafx.h"
#include "GameTimeManager.h"

#include <yojimbo/yojimbo.h>

void GameTimeManager::init(f64 fixedTimeStepSec)
{
    mTimeStepSec = fixedTimeStepSec;
    mTimeSec = yojimbo_time();
}

bool GameTimeManager::tryTick(f64* sleepSec)
{
    f64 currentTime = yojimbo_time();
    // Fixed ticks
    if (mTimeSec <= currentTime) {
        mTimeSec += mTimeStepSec;
        return true;
    }
    *sleepSec = mTimeSec - currentTime;
    return false;
}
