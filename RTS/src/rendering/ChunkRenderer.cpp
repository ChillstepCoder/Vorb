#include "stdafx.h"
#include "ChunkRenderer.h"
#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "World.h"
#include "ResourceManager.h"
#include "rendering/ChunkMesher.h"
#include "rendering/QuadMesh.h"
#include "rendering/TileVertex.h"
#include "rendering/ShaderLoader.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/RenderContext.h"
#include "rendering/ChunkGrassQuadtree.h"

// TODO: Remove
#include "Utils.h"

#include "options/DebugOptions.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>

#define ENABLE_DEBUG_RENDER 1
#if ENABLE_DEBUG_RENDER == 1
#include <Vorb/ui/InputDispatcher.h>
#endif

constexpr float FLORA_RENDER_DISTANCE_2 = SQ(320.0f);
constexpr float FLORA_UNLOAD_DISTANCE_2 = SQ(340.0f);
static_assert(FLORA_UNLOAD_DISTANCE_2 > FLORA_RENDER_DISTANCE_2);

ChunkRenderer::ChunkRenderer(const WorldGrid& worldGrid, const MaterialRenderer& materialRenderer) :
    mMaterialRenderer(materialRenderer),
    mMesher(std::make_unique<ChunkMesher>(worldGrid))
{
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::renderTiles(const World& world, const Camera3D& camera)
{

    // Tiles
    mMaterialRenderer.bindMaterialForRender(*mStandardMaterial);
    VGUniform offsetUniform = mStandardMaterial->mProgram.getUniform("unOffset");
    world.enumVisibleChunks([&](const Chunk& chunk) {
        ChunkRenderData& renderData = chunk.mChunkRenderData;
        f32v3 offset = chunk.getWorldPos3D() - camera.getPosition();
        glUniform3fv(offsetUniform, 1, &offset.x);
        TryRenderBaseMesh(chunk, mStandardMaterial);
    });
}

void ChunkRenderer::renderGrass(const World& world, const Camera3D& camera, const f32v3& playerPos)
{
    mMaterialRenderer.bindMaterialForRender(*mGrassMaterial);
    VGUniform offsetUniform = mGrassMaterial->mProgram.getUniform("unOffset");
    VGUniform fadeUniform = mGrassMaterial->mProgram.getUniform("unFadeDistance");
    glUniform3fv(mGrassMaterial->mProgram.getUniform("unPlayerPos"), 1, &playerPos.x);
    glUniform1f(fadeUniform, sDebugOptions.mGrassSettings.fadeDistance);
    world.enumVisibleChunks([&](const Chunk& chunk) {
        ChunkRenderData& renderData = chunk.mChunkRenderData;
        f32v3 offset = chunk.getWorldPos3D() - camera.getPosition();
        glUniform3fv(offsetUniform, 1, &offset.x);
        TryRenderGrassMeshes(chunk, mGrassMaterial, camera);
    });
}

void ChunkRenderer::renderBillboards(const World& world, const Camera3D& camera)
{
    mMaterialRenderer.bindMaterialForRender(*mBillboardMaterial);
    VGUniform offsetUniform = mBillboardMaterial->mProgram.getUniform("unOffset");
    world.enumVisibleChunks([&](const Chunk& chunk) {
        if (chunk.isFinished()) {

            mMesher->updateMesh(chunk, f32v3(world.getLoadCenter(), 0.0f));

            ChunkRenderData& renderData = chunk.mChunkRenderData;
            if (renderData.mBillboardMesh && renderData.mBillboardMesh->isValid()) {
                f32v3 offset = chunk.getWorldPos3D() - camera.getPosition();
                glUniform3fv(offsetUniform, 1, &offset.x);
                TryRenderBillboardMesh(chunk, mBillboardMaterial);
            }
        }
    });
}

void ChunkRenderer::renderWorldShadows(const World& world, const Camera3D& camera, f32 maxDistance) {
    // Render region LODs first due to depth sort
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);

    mMaterialRenderer.bindMaterialForRender(*mShadowMapperMaterial);
    world.enumVisibleChunks([&](const Chunk& chunk) {
        if (chunk.isFinished()) {
            if (glm::length2(chunk.getWorldPosCenter3D() - camera.getPosition()) <= maxDistSQ) {
                f32v3 offset = chunk.getWorldPos3D() - camera.getPosition();
                glUniform3fv(mShadowMapperMaterial->mProgram.getUniform("unOffset"), 1, &offset.x);
                TryRenderBaseMesh(chunk, mShadowMapperMaterial);
            }
        }
    });
}

void ChunkRenderer::TryRenderBaseMesh(const Chunk& chunk, const Material* material) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mChunkMesh && renderData.mChunkMesh->isValid()) {
        renderData.mChunkMesh->draw();
    }
}

void ChunkRenderer::TryRenderGrassMeshes(const Chunk& chunk, const Material* material, const Camera3D& camera) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mGrassLod) {
        renderData.mGrassLod->render(camera, material->mProgram);
    }
}

void ChunkRenderer::TryRenderBillboardMesh(const Chunk& chunk, const Material* material) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mBillboardMesh && renderData.mBillboardMesh->isValid()) {
        renderData.mBillboardMesh->draw();
    }
}


void ChunkRenderer::InitPostLoad() {
    const MaterialManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
    mStandardMaterial = materialManager.getMaterial("standard_tile");
    mGrassMaterial = materialManager.getMaterial("grass");
    mBillboardMaterial = materialManager.getMaterial("billboard_ssbo");
    mShadowMapperMaterial = materialManager.getMaterial("shadow_mapper");
    mShadowMapperMaterialBillboard = materialManager.getMaterial("shadow_mapper");
}
