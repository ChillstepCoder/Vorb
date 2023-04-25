#pragma once


#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"

class CliWorld : public IWorld, public CliWorldInterface
{
    friend class WorldFactory;
protected:
    CliWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid);

public:
    void tick(f32 elapsedSec);
    
    void onFrameBegin() override;
    void frameUpdate(const Camera3D& camera, f32 elapsedSec) override;
    void onWorldBegin(const f32v2& loadCenter) override;
    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) override;


    WorldNetMode getNetMode() override;
    WorldType getWorldType() override;

private:

};

