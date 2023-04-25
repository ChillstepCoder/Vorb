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
	HostWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid);

public:

	void tick(f32 elapsedSec);
	// IWorld interface
	void onFrameBegin() override;
	void frameUpdate(const Camera3D& camera, f32 elapsedSec) override;
	void onWorldBegin(const f32v2& loadCenter) override;
    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) override;

	WorldNetMode getNetMode() override;
	WorldType getWorldType() override;

private:

};

