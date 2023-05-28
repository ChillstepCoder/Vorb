#include "stdafx.h"
#include "FishEcosystem.h"

#include "world/IWorld.h"

FishEcosystem::FishEcosystem(IWorld& world) : mWorld(world) {

}

FishEcosystem::~FishEcosystem() {

}

void FishEcosystem::tickGameThread()
{
   const f32v2 loadCenter = mWorld.getLoadCenter();
}
