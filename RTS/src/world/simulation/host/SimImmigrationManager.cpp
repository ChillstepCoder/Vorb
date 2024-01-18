#include "stdafx.h"
#include "SimImmigrationManager.h"

SimImmigrationManager::SimImmigrationManager(HostSimContext& simContext) : mHostSimContext(simContext) {

}

SimImmigrationManager::~SimImmigrationManager() {

}

void SimImmigrationManager::tickSimThread(TimestampMs currentTime) {
    const ui64 timeDelta = currentTime - mLastTickTimestamp;
    mLastTickTimestamp = currentTime;


    assert(false);
}
