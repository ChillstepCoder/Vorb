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
static_assert(FLORA_UNLOAD_DISTANCE_2 > FLORA_RENDER_DISTANCE_2);

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer),
    mMesher(std::make_unique<ChunkMesher>(resourceManager.getTextureAtlas()))
{
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::renderChunksZCutout(const World& world, const Camera3D& camera)
{
    //world.enumVisibleChunks([&](const Chunk& chunk) {
    //    if (chunk.isFinished()) {
    //        // Check chunk mesh for update
    //        mMesher->updateMesh(chunk, f32v3(world.getLoadCenter(), 0.0f));

    //        // Only render 
    //        ChunkRenderData& renderData = chunk.mChunkRenderData;
    //        if (renderData.mChunkMesh && renderData.mChunkMesh->isValid()) {
    //            RenderContext::getInstance().getMaterialRenderer().renderMesh(*renderData.mChunkMesh, *mZCutoutMaterial);
    //        }
    //    }
    //});
}

void ChunkRenderer::renderWorld(const World& world, const Camera3D& camera, ChunkRenderLOD lod)
{

    ChunkID chunkId;

    if (lod == ChunkRenderLOD::FULL_DETAIL) {

        // Render region LODs first due to depth sort
        ui32 nextTextureIndex;
        mLODedChunksToRender.clear();

        // NEW
        // Billboards first to reduce overdraw
        mMaterialRenderer.bindMaterialForRender(*mBillboardMaterial);
        world.enumVisibleChunks([&](const Chunk& chunk) {
            if (chunk.isFinished()) {

                mMesher->updateMesh(chunk, f32v3(world.getLoadCenter(), 0.0f));

                ChunkRenderData& renderData = chunk.mChunkRenderData;
                if (renderData.mChunkMesh && renderData.mChunkMesh->isValid()) {
                    TryRenderBillboardMesh(chunk, mBillboardMaterial);
                }
                else {
                    mLODedChunksToRender.emplace_back(&chunk);
                }
            }
        });
        mMaterialRenderer.bindMaterialForRender(*mStandardMaterial);
        world.enumVisibleChunks([&](const Chunk& chunk) {
            ChunkRenderData& renderData = chunk.mChunkRenderData;
            f32v3 offset = chunk.getWorldPos3D() - camera.getPosition();
            glUniform3fv(glGetUniformLocation(mStandardMaterial->mProgram.getID(), "unOffset"), 1, &offset.x);
            TryRenderFloraMesh(chunk, mStandardMaterial);
            TryRenderBaseMesh(chunk, mStandardMaterial);
        });

        // LODed chunks
        mMaterialRenderer.bindMaterialForRender(*mLODMaterial, &nextTextureIndex);
        for (auto&& chunk : mLODedChunksToRender) {
            ChunkRenderData& renderData = chunk->mChunkRenderData;
            RenderLODTextureBindless(chunk->getWorldPos(), renderData.mLODTexture, CHUNK_WIDTH, camera, nextTextureIndex);
        }
        // OLD
        //world.enumVisibleChunks([&](const Chunk& chunk) {
        //    if (chunk.isFinished()) {
        //        // Check chunk mesh for update
        //        UpdateMesh(chunk, camera);

        //        RenderMeshOrLODTexture(chunk, camera);
        //    }
        //});

        mMaterialRenderer.bindMaterialForRender(*mLODMaterial, &nextTextureIndex);
        //vg::DepthState::NONE.set();
        world.enumVisibleRegions(camera, [&](const Region& region) {
            RenderLODTextureBindless(region.getWorldPos(), region.mRenderData.mLODTexture, WorldData::REGION_WIDTH_TILES, camera, nextTextureIndex);
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

        /* for (const Chunk* chunk : chunksNeedingUpdate) {
             if (!mMesher->createLODTextureAsync(*chunk)) {
                 break;
             }
         }*/

        // Render region LODs
        world.enumVisibleRegions(camera, [&](const Region& region) {
            RenderLODTextureBindless(region.getWorldPos(), region.mRenderData.mLODTexture, WorldData::REGION_WIDTH_TILES, camera, nextTextureIndex);
        });
    }
    static_assert((int)ChunkRenderLOD::COUNT == 2, "Update for new rendering style");
}

void ChunkRenderer::renderWorldShadows(const World& world, const Camera3D& camera, ChunkRenderLOD lod, f32 maxDistance) {
    assert(lod == ChunkRenderLOD::FULL_DETAIL);

    // Render region LODs first due to depth sort
    const f32 maxDistSQ = SQ(maxDistance + CHUNK_WIDTH * 0.5f);

    mMaterialRenderer.bindMaterialForRender(*mShadowMapperMaterial);
    world.enumVisibleChunks([&](const Chunk& chunk) {
        if (chunk.isFinished()) {
            if (glm::length2(chunk.getWorldPosCenter3D() - camera.getPosition()) <= maxDistSQ) {
                f32v3 offset = chunk.getWorldPos3D() - camera.getPosition();
                glUniform3fv(glGetUniformLocation(mShadowMapperMaterial->mProgram.getID(), "unOffset"), 1, &offset.x);
                TryRenderBaseMesh(chunk, mShadowMapperMaterial);
            }
        }
    });
}

//void ChunkRenderer::renderWorldShadows(const World& world)
//{
//    world.enumVisibleChunks([&](const Chunk& chunk) {
//        if (chunk.isFinished()) {
//            RenderShadows(chunk, camera);
//        }
//    });
//}


void ChunkRenderer::TryRenderBaseMesh(const Chunk& chunk, const Material* material) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mChunkMesh && renderData.mChunkMesh->isValid()) {
        renderData.mChunkMesh->draw(material->mProgram);
    }
}

void ChunkRenderer::TryRenderFloraMesh(const Chunk& chunk, const Material* material) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mHighDetailFloraMesh && renderData.mHighDetailFloraMesh->isValid()) {
        renderData.mHighDetailFloraMesh->draw(material->mProgram);
    }
}

void ChunkRenderer::TryRenderBillboardMesh(const Chunk& chunk, const Material* material) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (renderData.mBillboardMesh && renderData.mBillboardMesh->isValid()) {
        renderData.mBillboardMesh->draw(material->mProgram);
    }
}

//
//void ChunkRenderer::RenderMeshOrLODTexture(const Chunk& chunk, const Camera3D& camera) {
//	// mutable render data
//    ChunkRenderData& renderData = chunk.mChunkRenderData;
//    if (renderData.mChunkMesh && renderData.mChunkMesh->isValid()) {
//        MaterialRenderer& renderer = RenderContext::getInstance().getMaterialRenderer();
//        if (renderData.mHighDetailFloraMesh && renderData.mHighDetailFloraMesh->isValid()) {
//            renderer.renderMesh(*renderData.mHighDetailFloraMesh, *mStandardMaterial);
//        }
//        renderer.renderMesh(*renderData.mChunkMesh, *mStandardMaterial);
//        if (renderData.mBillboardMesh && renderData.mBillboardMesh->isValid()) {
//            renderer.renderMesh(*renderData.mBillboardMesh, *mBillboardMaterial);
//        }
//    }
//    else {
//        RenderLODTexture(chunk.getWorldPos(), renderData.mLODTexture, CHUNK_WIDTH, camera);
//    }
//}

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

void ChunkRenderer::InitPostLoad()
{
	mStandardMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
#if USE_INSTANCED_BILLBOARDS == 1
    mBillboardMaterial = mResourceManager.getMaterialManager().getMaterial("tbo_billboard");
#else
    mBillboardMaterial = mResourceManager.getMaterialManager().getMaterial("billboard");
#endif
    mLODMaterial = mResourceManager.getMaterialManager().getMaterial("chunk_lod");
    mZCutoutMaterial = mResourceManager.getMaterialManager().getMaterial("z_cutout");
    mShadowMapperMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_mapper");
    mShadowMapperMaterialBillboard = mResourceManager.getMaterialManager().getMaterial("shadow_mapper");
}
