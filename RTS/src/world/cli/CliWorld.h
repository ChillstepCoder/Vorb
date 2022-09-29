#pragma once


#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"

class CliWorld : public IWorld, public CliWorldInterface
{
public:
    CliWorld();

    void onFrameBegin() override;
    void frameUpdate(const Camera3D& camera, f32 elapsedSec) override;
    void initPostResourcesLoaded() override;

private:

};

