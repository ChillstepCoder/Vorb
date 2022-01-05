#include "stdafx.h"
#include "ChunkGrassQuadtree.h"

#include "rendering/QuadMesh.h"
#include "world/Chunk.h"
#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "services/Services.h"

#include "generation/WorldGenerationData.h"
#include <Vorb/graphics/GLProgram.h>

#include "Random.h"
#include "DebugRenderer.h"

constexpr int GRASS_LOD_DETAIL[GRASS_QUADTREE_MAX_LOD] = {
    0,
    1,
    3,
    6,
    12,
};

constexpr f32 GRASS_BLADE_WIDTHS[GRASS_QUADTREE_MAX_LOD] = {
    0.0f,
    1.00f,
    0.5f,
    0.12f,
    0.05f,
};

constexpr f32 GRASS_SUBDIVIDE_DISTANCES_SQ[GRASS_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    FLT_MAX,
    SQ(91.0f),
    SQ(46.0f),
    SQ(23.0f),
    -FLT_MAX // Never subdivide last
};


ChunkGrassQuadtree::ChunkGrassQuadtree(const Chunk& chunk) : mChunk(chunk), FlatQuadtree(chunk.getWorldPos(), GRASS_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mGrassSettings) {
    mWorldPos = mChunk.getWorldPos();
    mChunk.incRef();
}


ChunkGrassQuadtree::~ChunkGrassQuadtree()
{
    mChunk.decRef();
}

bool isPatchInRange(const f32v2& centerPos, const f32v2& cameraPos, f32 radius) {
    return (length2(centerPos - cameraPos) - SQ(radius)) <= sDebugOptions.mGrassSettings.distanceSq - SQ(CHUNK_WIDTH * 0.5f); // SQ chunkwidth half will make it fit more closely (for some reason?)
}


void ChunkGrassQuadtree::render(const Camera3D& camera, const vg::GLProgram& program) {
    const f32v3& cameraPos = camera.getPosition();
    const f32v2 cameraPos2Drelative = f32v2(cameraPos.x, cameraPos.y) - mChunk.getWorldPos();
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha"); // TODO: Cache?
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    f32v3 pos = mChunk.getWorldPos3D();
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        QuadtreePatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mMeshes[index];
            ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
            f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
            if (patch.isCrossfading()) {
                glUniform1f(crossfadeAlphaUniform, mCrossfadeTable[patch.mCrossFadeTableIndex] * 0.5f /* Constant that was selected via trial and error*/);
                glUniform1f(crossfadeDirectionUniform, patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_IN ? 1.0f : 0.0f);
            }
            else {
                glUniform1f(crossfadeAlphaUniform, 0.0f);
                glUniform1f(crossfadeDirectionUniform, 0.0f);
            }
            const f32 radius = LOD_RADIUS_DIMS[lod];
            if (camera.sphereIsVisible(centerPos3d + pos, radius) &&
                isPatchInRange(centerPos, cameraPos2Drelative, radius)) {
                mesh->draw(program);
            }
        }
    }
}

void createGrassMesh(
    GrassBillboardMesh& grassMesh,
    const Chunk& chunk,
    const ui32v2& tilePosStart,
    ui32 lod
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::LOD_DIMS[lod];
    const ui32 density = GRASS_LOD_DETAIL[lod];
    const f32 bladeWidth = GRASS_BLADE_WIDTHS[lod];
    grassMesh.reserveQuadCount((size_t)dims.x * dims.y * density * density);
    for (ui32 y = 0; y < dims.y; ++y) {
        for (ui32 x = 0; x < dims.x; ++x) {
            TileIndex tileIndex(tilePosStart.x + x, tilePosStart.y + y);

            ui8 grassVal = chunk.getGrassAt(tileIndex);
            if (grassVal == 0) {
                continue;
            }

            const f32v2 tileWorldPos = f32v2(tileIndex.getX(), tileIndex.getY());

            /*Tile neighbors[8];
            chunk.getTileNeighbors(tileIndex, neighbors);

            const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
            const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
            const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

            const int tx = tileIndex.getX();
            const int ty = tileIndex.getY();
            // Allow overlap when adjacent tiles are the same
            //const float rightXMult = (rightTile.baseZPosition != tile.baseZPosition || tileId != rightTile.layers[layerIndex]) ? 1.0f : 0.0f;
            //const float topXMult = (topTile.baseZPosition != tile.baseZPosition || tileId != topTile.layers[layerIndex]) ? 1.0f : 0.0f;

            // Handle variant UVs
            constexpr int NUM_GRASS_TYPES = 4;

            // TODO: Determine edge


            // Generate blades
            for (int y2 = 0; y2 < (int)density; ++y2) {
                for (int x2 = 0; x2 < (int)density; ++x2) {
                    const f32 rnd = Random::getCachedRandomfSpecific(x2 + CHUNK_SIZE * y2 - tx - ty * CHUNK_SIZE) * 0.9f;
                    const float xo = (x2 + rnd) / (float)density;
                    const float yo = (y2 - rnd) / (float)density;
                    const float rsize = lerp(0.4f, 0.6f, rnd);
                    const f32 grassNoise = -sWorldGenData.mGrassNoise.compute((f64)tileWorldPos.x + xo + chunk.getWorldPos().x, (f64)tileWorldPos.y + yo + chunk.getWorldPos().y);
                    const ui8 variantIndex = (ui8)((grassNoise + 1.0f) * SQ(NUM_GRASS_TYPES)) % NUM_GRASS_TYPES;
                    grassMesh.addBladeQuad(
                        f32v3(tileWorldPos.x + xo, tileWorldPos.y + yo, 0.0f), // TODO: new height
                        f32v2(bladeWidth, rsize),
                        variantIndex
                    );
                }
            }
        }
    }
};

void ChunkGrassQuadtree::updateMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) {

    patch.mFlags &= (~QUADTREE_PATCH_FLAG_DIRTY_MESH);
    patch.mFlags |= QUADTREE_PATCH_FLAG_MESHING;
    if (!mMeshes[patchIndex]) {
        mMeshes[patchIndex] = std::make_unique<GrassBillboardMesh>();
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    ++mRefCount;
    mChunk.incRef();

    assert(!patch.isCrossfading() && /*!patch.isMeshing() &&*/ !patch.isMeshDirty() && patch.isActive());

    Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex](ThreadPoolWorkerData*) {

        PreciseTimer timer;

        createGrassMesh(*mMeshes[patchIndex], mChunk, PATCH_POSITIONS.data[patchIndex].xy, lod);

        std::cout << "GRASS: " << lod << " " << timer.stop() << std::endl;
    }, [this, &patch, patchIndex]() {

        mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
        patch.mFlags &= (~QUADTREE_PATCH_FLAG_MESHING);
        if (mMeshes[patchIndex]->isValid()) {
            patch.mFlags |= QUADTREE_PATCH_FLAG_HAS_MESH;
        }
        else {
            patch.mCrossFadeTableIndex &= (~QUADTREE_PATCH_FLAG_HAS_MESH);
            mMeshes[patchIndex].reset(); // Free the memory
        }

        // If recombining we wont update till next cycle
        if (patch.mStatus != QUADTREE_PATCH_STATUS_RECOMBINING) {
            patch.mStatus = QUADTREE_PATCH_STATUS_VALID;

            if (!getQuadtreeParent(patchIndex, mNodes).isActive()) {
                // If our parent isnt active or we arent recombining, then we can render, otherwise we will wait for parent to deactivate
                patch.mFlags |= QUADTREE_PATCH_FLAG_SHOULD_RENDER;
            }
        }

        // Update refcount
        --mRefCount;
        mChunk.decRef();
    });
}

void ChunkGrassQuadtree::freeMeshForPatch(ui32 patchIndex) {
    mMeshes[patchIndex].reset();
}
