#include "stdafx.h"
#include "SimECS.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "world/simulation/host/system/SimAISystem.h"


SimECS::SimECS(HostSimContext& hostSimContext) : mHostSimContext(hostSimContext) {

}

SimECS::~SimECS() {

}

void SimECS::tickSimThread(TimestampMs currentTimestamp) {
    mCurrentTickTimestamp = currentTimestamp;
    mTimeDelta = currentTimestamp - mLastTickTimestamp;
    mLastTickTimestamp = currentTimestamp;
    
    updateAI();
    updateSettlements();
}

void SimECS::updateAI() {
    SimAISystem::tick(mRegistry, mCurrentTickTimestamp, mTimeDelta);
}

void SimECS::updateSettlements()
{

}
