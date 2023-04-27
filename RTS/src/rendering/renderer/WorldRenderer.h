#pragma once

#include "world/WorldType.h"

class AmbientOcclusionPostProcess;
class CharacterRenderer;
class CityDebugRenderer;
class CloudManager;
class CloudRenderer;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class GrassRenderer;
class InstancedStaticModelRenderer;
class ItemRenderer;
class IWorld;
class LightRenderer;
class ParticleSystemRenderer;
class ShadowRenderer;
class SmudgeRenderer;
class TerrainRenderer;
class TileContainerRenderer;
class TonemapRenderer;

class WorldRenderer
{
public:
    WorldRenderer(IWorld& world);
    ~WorldRenderer();

private:
   /* mutable std::unique_ptr<TileContainerRenderer> mTileContainerRenderer;
    mutable std::unique_ptr<LightRenderer> mLightRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;
    mutable std::unique_ptr<ParticleSystemRenderer> mParticleSystemRenderer;
    mutable std::unique_ptr<CityDebugRenderer> mCityDebugRenderer;
    mutable std::unique_ptr<ItemRenderer> mItemRenderer;
    mutable std::unique_ptr<CharacterRenderer> mCharacterRenderer;
    mutable std::unique_ptr<CloudRenderer> mCloudRenderer;
    mutable std::unique_ptr<DepthOfFieldPostProcess> mDepthOfField;
    mutable std::unique_ptr<AmbientOcclusionPostProcess> mAmbientOcclusion;
    mutable std::unique_ptr<ShadowRenderer> mShadowRenderer;
    mutable std::unique_ptr<TerrainRenderer> mTerrainRenderer;
    mutable std::unique_ptr<GrassRenderer> mGrassRenderer;
    mutable std::unique_ptr<InstancedStaticModelRenderer> mStaticModelRenderer;
    mutable std::unique_ptr<SmudgeRenderer> mSmudgeRenderer;
    mutable std::unique_ptr<TonemapRenderer> mTonemapRenderer;
    std::unique_ptr<CloudManager> mCloudManager;*/

    const IWorld& mWorld;
};

