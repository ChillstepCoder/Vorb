#include "stdafx.h"
#include "GameTimeManager.h"

#include <yojimbo/yojimbo.h>

void GameTimeManager::init(f64 fixedTimeStepSec) {
    assert(IS_GAME_THREAD());
    mTimeStepSec = fixedTimeStepSec;
    mTimeSec = yojimbo_time();
}

bool GameTimeManager::tryTick(f64* sleepSec) {
    assert(IS_GAME_THREAD());
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
