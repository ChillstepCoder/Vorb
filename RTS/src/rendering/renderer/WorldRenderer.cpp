#include "stdafx.h"
#include "WorldRenderer.h"

#include "world/IWorld.h"

WorldRenderer::WorldRenderer(IWorld& world) : mWorld(world)
{
   /* mCharacterRenderer = std::make_unique<CharacterRenderer>();
    mStaticModelRenderer = std::make_unique<InstancedStaticModelRenderer>();
    mTileContainerRenderer = std::make_unique<TileContainerRenderer>(*mStaticModelRenderer);
    mLightRenderer = std::make_unique<LightRenderer>();
    mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>();
    mParticleSystemRenderer = std::make_unique<ParticleSystemRenderer>(mScreenResolution);
    mCityDebugRenderer = std::make_unique<CityDebugRenderer>();
    mItemRenderer = std::make_unique<ItemRenderer>();
    mCloudRenderer = std::make_unique<CloudRenderer>(mScreenResolution);
    mDepthOfField = std::make_unique<DepthOfFieldPostProcess>(mScreenResolution);
    mAmbientOcclusion = std::make_unique<AmbientOcclusionPostProcess>(mScreenResolution);
    mShadowRenderer = std::make_unique<ShadowRenderer>(mScreenResolution);
    mTerrainRenderer = std::make_unique<TerrainRenderer>();
    mGrassRenderer = std::make_unique<GrassRenderer>();
    mSmudgeRenderer = std::make_unique<SmudgeRenderer>(mScreenResolution);
    mTonemapRenderer = std::make_unique<TonemapRenderer>();
    checkGlError("Renderer init");

    mCloudManager->init(world.getLoadCenter());*/
}

WorldRenderer::~WorldRenderer()
{
}
