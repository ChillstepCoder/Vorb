#include "stdafx.h"
#include "HostSimContext.h"

#include "world/simulation/host/SimThread.h"

HostSimContext::HostSimContext(World& world) : WorldContextObject(world) {
    mSimThread = std::make_unique<SimThread>(*this, world);
}

HostSimContext::~HostSimContext()
{

}
