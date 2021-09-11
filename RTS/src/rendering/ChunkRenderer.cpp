#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
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
#include "services/Services.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>

#define ENABLE_DEBUG_RENDER 1
#if ENABLE_DEBUG_RENDER == 1
#include <Vorb/ui/InputDispatcher.h>
#endif

constexpr float FLORA_RENDER_DISTANCE_2 = SQ(320.0f);
constexpr float FLORA_UNLOAD_DISTANCE_2 = SQ(340.0f);

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
	mResourceManager(resourceManager),
	mMaterialRenderer(materialRenderer)
{
    mMesher = std::make_unique<ChunkMesher>(resourceManager.getTextureAtlas());
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::renderChunksZCutout(const World& world, const Camera3D& camera)
{
    world.enumVisibleChunks([&](const Chunk& chunk) {
        if (chunk.isFinished()) {
            // Check chunk mesh for update
            UpdateMesh(chunk, camera);

            // Only render 
            ChunkRenderData& renderData = chunk.mChunkRenderData;
            if (renderData.mChunkMesh) {
                RenderContext::getInstance().getMaterialRenderer().renderMesh(*renderData.mChunkMesh, *mZCutoutMaterial);
            }
        }
    });
}

void ChunkRenderer::renderWorld(const World& world, const Camera3D& camera, ChunkRenderLOD lod)
{
#if ENABLE_DEBUG_RENDER == 1
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_R)) {
        if (!s_wasTogglePressed) {
            s_wasTogglePressed = true;
            s_debugToggle = !s_debugToggle;
        }
    }
    else {
        s_wasTogglePressed = false;
    }
#endif

    ChunkID chunkId;
    const Chunk* chunk;

    if (lod == ChunkRenderLOD::FULL_DETAIL) {

        // Render region LODs first due to depth sort
        ui32 nextTextureIndex;
        mMaterialRenderer.bindMaterialForRender(*mLODMaterial, &nextTextureIndex);
        //vg::DepthState::NONE.set();
        world.enumVisibleRegions(camera, [&](const Region& region) {
            RenderLODTextureBindless(region.getWorldPos(), region.mRenderData.mLODTexture, WorldData::REGION_WIDTH_TILES, camera, nextTextureIndex);
        });
        //vg::DepthState::FULL.set();

        world.enumVisibleChunks([&](const Chunk& chunk) {
            if (chunk.isFinished()) {
                // Check chunk mesh for update
                UpdateMesh(chunk, camera);

                RenderMeshOrLODTexture(chunk, camera);
            }
        });
        
    }
    else {

        ui32 nextTextureIndex;
        mMaterialRenderer.bindMaterialForRender(*mLODMaterial, &nextTextureIndex);

        // Render all chunks
        std::vector<const Chunk*> chunksNeedingUpdate;
        world.enumVisibleChunks([&](const Chunk& chunk) {
            if (chunk.isFinished()) {
                // Check LOD for update
                //UpdateLODTexture(chunk);
                ChunkRenderData& renderData = chunk.mChunkRenderData;
                if (renderData.mLODDirty && !renderData.mIsBuildingBaseMesh) {
                    chunksNeedingUpdate.push_back(&chunk);
                }

                RenderLODTextureBindless(chunk.getWorldPos(), renderData.mLODTexture, CHUNK_WIDTH, camera, nextTextureIndex);
            }
        });

        // TODO: We could cache the distances if we cared
        std::sort(chunksNeedingUpdate.begin(), chunksNeedingUpdate.end(), [&](const Chunk* a, const Chunk* b) { 
            const f32v2& center = camera.getPosition();
            const float dista2 = glm::length2(a->getWorldPos() - center);
            const float distb2 = glm::length2(b->getWorldPos() - center);
            return dista2 < distb2;
        });

        for (const Chunk* chunk : chunksNeedingUpdate) {
            if (!mMesher->createLODTextureAsync(*chunk)) {
                break;
            }
        }

        // Render region LODs
        world.enumVisibleRegions(camera, [&](const Region& region) {
            RenderLODTextureBindless(region.getWorldPos(), region.mRenderData.mLODTexture, WorldData::REGION_WIDTH_TILES, camera, nextTextureIndex);
        });
    }
    static_assert((int)ChunkRenderLOD::COUNT == 2, "Update for new rendering style");
}

//void ChunkRenderer::renderWorldShadows(const World& world)
//{
//    world.enumVisibleChunks([&](const Chunk& chunk) {
//        if (chunk.isFinished()) {
//            RenderShadows(chunk, camera);
//        }
//    });
//}

void ChunkRenderer::UpdateMesh(const Chunk& chunk, const Camera3D& camera) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mIsBuildingBaseMesh && renderData.mMeshDirty) {
         mMesher->createMeshAsync(chunk);
    }

    if (!renderData.mIsBuildingHighDetailFloraMesh) {
        const f32 distanceToCamera2 = glm::length2(camera.getPosition() - chunk.getWorldPosCenter3D());
        if (distanceToCamera2 < FLORA_RENDER_DISTANCE_2 && renderData.mHighDetailFloraMeshDirty) {
            mMesher->createHighDetailFloraMeshAsync(chunk);
        }
        else if (distanceToCamera2 > FLORA_UNLOAD_DISTANCE_2 && renderData.mHighDetailFloraMesh) {
            renderData.mHighDetailFloraMesh.reset();
        }
    }
}

void ChunkRenderer::UpdateHighDetailFloraMesh(const Chunk& chunk) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mIsBuildingHighDetailFloraMesh && renderData.mLODDirty) {
        mMesher->createHighDetailFloraMeshAsync(chunk);
    }
}

void ChunkRenderer::UpdateLODTexture(const Chunk& chunk) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mIsBuildingBaseMesh && renderData.mLODDirty) {
        mMesher->createLODTextureAsync(chunk);
    }
}

void ChunkRenderer::RenderMeshOrLODTexture(const Chunk& chunk, const Camera3D& camera) {
	// mutable render data
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mChunkMesh) {
        MaterialRenderer& renderer = RenderContext::getInstance().getMaterialRenderer();
        if (renderData.mHighDetailFloraMesh) {
            renderer.renderMesh(*renderData.mHighDetailFloraMesh, *mStandardMaterial);
        }
        renderer.renderMesh(*renderData.mChunkMesh, *mStandardMaterial);
        if (renderData.mBillboardMesh) {
            renderer.renderMesh(*renderData.mBillboardMesh, *mBillboardMaterial);
        }
        else {
            // TODO: I feel lazy is bad here...
            mMesher->createMeshAsync(chunk);
        }
    }
    else {
        RenderLODTexture(chunk.getWorldPos(), renderData.mLODTexture, CHUNK_WIDTH, camera);
    }
}

void ChunkRenderer::RenderLODTexture(const f32v2& worldPos, VGTexture texture, f32 width, const Camera3D& camera) {
    if (texture) {
        const f32v4 rect(worldPos.x, worldPos.y, width, width);
        RenderContext::getInstance().getMaterialRenderer().renderMaterialToQuadWithTexture(*mLODMaterial, texture, rect);
    }
}

void ChunkRenderer::RenderLODTextureBindless(const f32v2& worldPos, VGTexture texture, f32 width, const Camera3D& camera, ui32 textureIndex) {
    if (texture) {
        const f32v4 rect(worldPos.x, worldPos.y, width, width);
        RenderContext::getInstance().getMaterialRenderer().renderMaterialToQuadWithTextureBindless(*mLODMaterial, texture, textureIndex, rect);
    }
}

//void ChunkRenderer::RenderShadows(const Chunk& chunk)
//{
//    /* ChunkRenderData& renderData = chunk.mChunkRenderData;
//     QuadMesh* mesh = renderData.mChunkMesh.get();
//     if (mesh && mesh->isValid()) {
//         RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*mesh, *mShadowMaterial);
//
//         if (camera.getScale() > FLORA_RENDER_SCALE_THRESHOLD) {
//             if (renderData.mFloraMesh) {
//                 RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*renderData.mFloraMesh, *mFloraShadowMaterial);
//             }
//         }
//     }*/
//}

void ChunkRenderer::ReloadShaders() {
	// reload dirty via timestamp
	assert(false);
	//for (auto& material : mMaterials) {
		//material.dispose();
	//}
	InitPostLoad();
}

void ChunkRenderer::InitPostLoad()
{
	mStandardMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
    assert(mStandardMaterial);
    mBillboardMaterial = mResourceManager.getMaterialManager().getMaterial("billboard");
    assert(mBillboardMaterial);
    mLODMaterial = mResourceManager.getMaterialManager().getMaterial("chunk_lod");
    assert(mLODMaterial);
    mZCutoutMaterial = mResourceManager.getMaterialManager().getMaterial("z_cutout");
    assert(mZCutoutMaterial);
}
