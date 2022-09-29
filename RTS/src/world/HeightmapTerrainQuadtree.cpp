#include "stdafx.h"
#include "HeightmapTerrainQuadtree.h"

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"
#include "debugging/DebugRenderer.h"
#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"
#include "rendering/mesh/MeshBuilder.h"

#include "generation/WorldGeneration.h"
#include <Vorb/graphics/GLProgram.h>

constexpr f32 TERRAIN_SUBDIVIDE_DISTANCES_SQ[TERRAIN_QUADTREE_MAX_LOD] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    SQ(6000.0f),
    SQ(3000.0f),
    SQ(600.0f),
    SQ(64.0f),
    -FLT_MAX // Never subdivide last
};


HeightmapTerrainQuadtree::HeightmapTerrainQuadtree() : FlatQuadtree(f32v2(0.0f), TERRAIN_SUBDIVIDE_DISTANCES_SQ, sDebugOptions.mTerrainLodDistanceOffset) {

}

HeightmapTerrainQuadtree::~HeightmapTerrainQuadtree()
{

}

void HeightmapTerrainQuadtree::init(const f32v2& worldPosition)
{
    mWorldPos = worldPosition;
}

void HeightmapTerrainQuadtree::renderTerrain(const Camera3D& camera, const vg::GLProgram& program) const {
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha"); // TODO: Cache?
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    f32v3 pos3D(mWorldPos.x, mWorldPos.y, 0.0f);
    VGUniform offsetUniform = program.getUniform("unOffset");
    f32v3 offset = pos3D - camera.getPosition();
    glUniform3fv(offsetUniform, 1, &offset.x);

    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        const QuadtreePatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mTerrainMeshes[index];
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
            const BoundingSphere& bounds = mesh->getBoundingSphere();
            if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
                mesh->draw();
            }
        }
    }
}

void HeightmapTerrainQuadtree::renderWater(const Camera3D& camera, const vg::GLProgram& program) const {
    f32v3 pos3D(mWorldPos.x, mWorldPos.y, 0.0f);
    VGUniform offsetUniform = program.getUniform("unOffset");
    f32v3 offset = pos3D - camera.getPosition();
    glUniform3fv(offsetUniform, 1, &offset.x);

    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        const QuadtreePatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mWaterMeshes[index];
            ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
            f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
            if (patch.isCrossfading() && (patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_OUT)) {
                // We simply dont crossfade water, always render the in crossfade only
                continue;
            }
            const BoundingSphere& bounds = mesh->getBoundingSphere();
            if (camera.sphereIsVisible(bounds.center, bounds.radius)) {
                mesh->draw();
            }
        }
    }
}

void HeightmapTerrainQuadtree::markDirty() {
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        QuadtreePatch& patch = mNodes[index];
        patch.mFlags |= QUADTREE_PATCH_FLAG_DIRTY_MESH;
    }
}

void createTerrainAndWaterMesh(
    MeshBuilder& terrainBuilder,
    MeshBuilder& waterBuilder,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);

    // AABB calculation
    f32AABB3 aabb;
    aabb.dims.x = dims.x;
    aabb.dims.y = dims.y;
    aabb.pos.x = posStart.x + worldPos.x;
    aabb.pos.y = posStart.y + worldPos.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = FLT_MIN;

    // Generate heightfield
    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (ui32 y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++x) {
            const f32v2 vertPos = f32v2(posStart.x + ((f32)x - 1.0f) * quadDims.x, posStart.y + ((f32)y - 1.0f) * quadDims.y);
            f32 zPos = sWorldGen.getHeightAtPos(f32v2(vertPos.x + worldPos.x, vertPos.y + worldPos.y));
            if (zPos < minZ) minZ = zPos;
            if (zPos > maxZ) maxZ = zPos;
            paddedHeightfield[y][x] = zPos;
        }
    }
    terrainBuilder.setVertsTerrainFromPaddedHeightfield(posStart, dims.x, paddedHeightfield);
    waterBuilder.setVertsWaterFromPaddedHeightfield(posStart, dims.x, paddedHeightfield);
    // Bounding sphere
    aabb.pos.z = minZ;
    aabb.dims.z = maxZ - minZ;
    const BoundingSphere sphere = boundingSphereFromAABB(aabb);
    terrainBuilder.setBoundingSphere(sphere);
    waterBuilder.setBoundingSphere(sphere);
};

void createTerrainAndWaterMesh(
    MeshBuilder& terrainBuilder,
    MeshBuilder& waterBuilder,
    const ui32v2& posStart,
    ui32 lod,
    const f32v2& worldPos
) {
    const ui32v2& dims = (ui32v2&)FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::LOD_DIMS[lod];
    f32v2 quadDims = f32v2(dims) / f32v2(TERRAIN_MESH_WIDTH_QUADS);
    f32v2 patchWorldPos = worldPos + f32v2(posStart);
    ui32v2 intWorldPos(glm::round(patchWorldPos));

    const ui32 quadWidth = HEIGHTMAP_QUAD_SIZE;
    intWorldPos -= quadWidth; // Padding so we start on the side

    f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS];
    for (int y = 0; y < TERRAIN_MESH_PADDED_WIDTH_VERTS; ++y) {
        sHeightmapGrid->copyHeightRowToBuffer(paddedHeightfield[y], intWorldPos, TERRAIN_MESH_PADDED_WIDTH_VERTS);
        intWorldPos.y += quadWidth;
    }

    // Compute bounds
    // TODO: TRUE AABB generated bounding sphere via boundingSphereFromAABB
    BoundingSphere boundingSphere;
    f32 lodRadius = HeightmapTerrainQuadtree::LOD_DIMS[lod].x * 0.5f;
    f32v2 centerxy = patchWorldPos + f32v2(lodRadius);
    boundingSphere.center = f32v3(centerxy.x, centerxy.y, 0.0f);
    f32 lodRadius2 = SQ(lodRadius);
    boundingSphere.radius = sqrt(lodRadius2 + lodRadius2) + 10.0f;// 10.0f is tmp until true bounding sphere
    terrainBuilder.setBoundingSphere(boundingSphere);
    waterBuilder.setBoundingSphere(boundingSphere);

    // Build
    terrainBuilder.setVertsTerrainFromPaddedHeightfield(posStart, dims.x, paddedHeightfield);
    waterBuilder.setVertsWaterFromPaddedHeightfield(posStart, dims.x, paddedHeightfield);
};

void HeightmapTerrainQuadtree::buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex)
{
    bool hasAquired = true;
    if (!mTerrainMeshes[patchIndex]) {
        hasAquired = false; // If we dont have a mesh, we haven't aquired yet
        mTerrainMeshes[patchIndex] = std::make_unique<Mesh>();
        mWaterMeshes[patchIndex] = std::make_unique<Mesh>();
        assert(patch.mStatus == QUADTREE_PATCH_STATUS_INVALID || patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING);
    }
    ++mRefCount;
    assert(!patch.isCrossfading() && !patch.isMeshDirty() && patch.isActive());

    if (lod == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        // At highest LOD we ask the heightmap generator to handle it
        const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
        // Sentinal IDs never mesh
        if (id.isSentinelID()) {
            finishMeshes(nullptr, nullptr, patchIndex);
            return;
        }

        // TODO: Can we malloc these together?
        std::shared_ptr<MeshBuilder> terrainBuilder = std::make_shared<MeshBuilder>(true);
        std::shared_ptr<MeshBuilder> waterBuilder = std::make_shared<MeshBuilder>(true);

        if (hasAquired || sHeightmapGrid->tryAquirePaddedHeightDataAt(id)) {
            // Instantly generate
            Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, id, terrainBuilder, waterBuilder](ThreadPoolWorkerData*) {
                createMeshes(*terrainBuilder, *waterBuilder, id, patchIndex, lod);
            }, [this, &patch, patchIndex, terrainBuilder, waterBuilder]() {
                finishMeshes(terrainBuilder.get(), waterBuilder.get(), patchIndex);
            });
        }
        else {
            // Wait for the terrain generator to generate our chunk
            sHeightmapGrid->requestPaddedHeightDataGenAndAquireAt(id, [this, &patch, lod, patchIndex, id, terrainBuilder, waterBuilder]() {
                Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, id, terrainBuilder, waterBuilder](ThreadPoolWorkerData*) {
                    createMeshes(*terrainBuilder, *waterBuilder, id, patchIndex, lod);
                }, [this, &patch, patchIndex, terrainBuilder, waterBuilder]() {
                    finishMeshes(terrainBuilder.get(), waterBuilder.get(), patchIndex);
                });
            });
        }
    }
    else {
        // TODO: Can we malloc these together?
        std::shared_ptr<MeshBuilder> terrainBuilder = std::make_shared<MeshBuilder>(true);
        std::shared_ptr<MeshBuilder> waterBuilder = std::make_shared<MeshBuilder>(true);
        // At lower LODs we have to regenerate every time
        // TODO: we actually shouldnt do this.. it ignores diffs
        Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex, terrainBuilder, waterBuilder](ThreadPoolWorkerData*) {
            createTerrainAndWaterMesh(*terrainBuilder, *waterBuilder, PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos);
        }, [this, &patch, patchIndex, terrainBuilder, waterBuilder]() {
            finishMeshes(terrainBuilder.get(), waterBuilder.get(), patchIndex);
        });
    }
}

void HeightmapTerrainQuadtree::createMeshes(MeshBuilder& terrainMeshBuilder, MeshBuilder& waterMeshBuilder, const HeightmapPatchID id, ui32 patchIndex, ui32 lod) {
    createTerrainAndWaterMesh(terrainMeshBuilder, waterMeshBuilder, PATCH_POSITIONS.data[patchIndex].xy, lod, mWorldPos);
}

void HeightmapTerrainQuadtree::finishMeshes(MeshBuilder* terrainMeshBuilder, MeshBuilder* waterMeshBuilder, ui32 patchIndex) {
    if (terrainMeshBuilder) {
        terrainMeshBuilder->finishMesh(*mTerrainMeshes[patchIndex], MeshDrawMode::STATIC);
        waterMeshBuilder->finishMesh(*mWaterMeshes[patchIndex], MeshDrawMode::STATIC);
    }
    onMeshFinished(patchIndex, mTerrainMeshes[patchIndex]->isValid() || mWaterMeshes[patchIndex]->isValid());
    // Update refcount
    --mRefCount;
}



void HeightmapTerrainQuadtree::freeMeshForPatch(ui32 patchIndex)
{
    // Only highest LOD has reference to heightmap
    if (QUADTREE_LOD_FROM_INDEX[patchIndex] == FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::HIGHEST_LOD) {
        const HeightmapPatchID id = getHeightmapPatchID(patchIndex);
        sHeightmapGrid->releasePaddedHeightDataAt(id);
    }
    mTerrainMeshes[patchIndex].reset();
    mWaterMeshes[patchIndex].reset();
}
