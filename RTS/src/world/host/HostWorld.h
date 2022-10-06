#pragma once

#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"
#include "world/srv/SrvWorldInterface.h"

class CityGraph;
class StructureManager;
class ItemStockpileRegistry;
class CloudManager;

class HostWorld : public IWorld, public CliWorldInterface, public SrvWorldInterface {
    friend class WorldFactory;
protected:
	HostWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(chunkGrid, heightmapGrid) {}

public:

	void tick(const f32v2& playerPos, f32 elapsedSec);
	// IWorld interface
	void onFrameBegin() override;
	void frameUpdate(const Camera3D& camera, f32 elapsedSec) override;
	void onWorldBegin(const f32v2& loadCenter) override;

private:

};

