#pragma once

#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"
#include "world/srv/SrvWorldInterface.h"

class CityGraph;
class StructureManager;
class ItemStockpileRegistry;
class CloudManager;

class HostWorld : public IWorld, public CliWorldInterface, public SrvWorldInterface {
public:
	HostWorld();

	void tick(const f32v2& playerPos, f32 elapsedSec);
	// IWorld interface
	void onFrameBegin() override;
	void frameUpdate(const Camera3D& camera, f32 elapsedSec) override;
	void initPostResourcesLoaded() override;

private:

};

