#pragma once

class IWorld;

class FishRenderer
{
public:
    void debugRenderFishEcosystem(const IWorld& world);

private:
    int mDebugTickCounter = 0;
};

