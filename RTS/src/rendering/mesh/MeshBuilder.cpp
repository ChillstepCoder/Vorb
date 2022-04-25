#include "stdafx.h"
#include "MeshBuilder.h"

#include <boost/pool/singleton_pool.hpp>

constexpr ui32 WATER_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6;
constexpr ui32 TERRAIN_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6 + TERRAIN_MESH_WIDTH_QUADS * 4 * 6;

struct mesh_builder_pool {};
using singleton_task_pool = boost::singleton_pool<mesh_builder_pool, sizeof(MeshBuilder), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 128u>;

VGBuffer MeshBuilder::sQuadIbo = 0;
VGBuffer MeshBuilder::sTerrainIbo = 0;

void MeshBuilder::setVertsTerrainFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {
    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;
    std::vector<Vertex32>& terrainVerts = mMainSubMeshData.mVerts;
    terrainVerts.resize(TERRAIN_MESH_SIZE_VERTS);

    // We are a terrain mesh
    mPolyTypeFlags.setBit(PolyTypeFlags::TERRAIN);
    
    constexpr f32 NORMAL_STRENGTH = 1.0f / 4.0f;
    for (int y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            TerrainVertex& v = terrainVerts[y * TERRAIN_MESH_WIDTH_VERTS + x].mTerrain;
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
            v.pos.z = height;

            // Normal calc
            f32 fl = paddedHeightfield[y][x]; // front left
            f32  l = paddedHeightfield[y + 1][x];   // left
            f32 bl = paddedHeightfield[y + 2][x]; // back left
            f32  f = paddedHeightfield[y][x + 1];   // front
            f32  b = paddedHeightfield[y + 2][x + 1];   // back
            f32 fr = paddedHeightfield[y][x + 2]; // front right
            f32  r = paddedHeightfield[y + 1][x + 2];   // right
            f32 br = paddedHeightfield[y + 2][x + 2]; // back right

            //https://gamedev.stackexchange.com/questions/165575/calculating-normal-map-from-height-map-using-sobel-operator
            // Sobel filter
            const f32 dX = (fl + 2.0f * l + bl) - (fr + 2.0f * r + br);
            const f32 dY = (fl + 2.0f * f + fr) - (bl + 2.0f * b + br);
            const f32 dZ = quadWidth;

            f32v3 n(dX, dY, dZ);
            v.normal = glm::normalize(n);
        }
    }

    ui32 index = SQ(TERRAIN_MESH_WIDTH_VERTS);
    const float SKIRT_DEPTH = quadWidth * 2.0f;
    // Build skirts
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the top edge
        v = terrainVerts[i].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Left Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the left edge
        v = terrainVerts[i * TERRAIN_MESH_WIDTH_VERTS].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Right Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the right edge
        v = terrainVerts[i * TERRAIN_MESH_WIDTH_VERTS + TERRAIN_MESH_WIDTH_VERTS - 1].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
    // Bottom Skirt
    for (int i = 0; i < TERRAIN_MESH_WIDTH_VERTS; i++) {
        TerrainVertex& v = terrainVerts[index++].mTerrain;
        // Copy the vertices from the bottom edge
        v = terrainVerts[TERRAIN_MESH_WIDTH_VERTS_SQ - TERRAIN_MESH_WIDTH_VERTS + i].mTerrain;
        // Extrude downward
        v.pos.z -= SKIRT_DEPTH;
    }
}

void MeshBuilder::setVertsWaterFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS])
{
    std::vector<Vertex32>& waterVerts = mMainSubMeshData.mVerts;
    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;
    waterVerts.resize(TERRAIN_MESH_SIZE_VERTS);
    mPolyTypeFlags.setBit(PolyTypeFlags::WATER);

    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            WaterVertex& v = waterVerts[y * TERRAIN_MESH_WIDTH_VERTS + x].mWater;
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
            v.pos.z = 0.0f;
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.depth = glm::max(-height, 0.0f);
        }
    }
}

void MeshBuilder::finishMesh(Mesh& mesh, MeshDrawMode drawMode) {

    // return blank mesh if we have no geometry
    if (mMainSubMeshData.mVerts.empty()) {
        return;
    }

    // Set bounds
    mesh.mBoundingSphere = mBoundingSphere;

    // Check if we need to destroy some old submeshes
    const bool wasUsingSharedIbo = mesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO);
    if (mesh.mSubMeshes.size() > mSubMeshesData.size()) {
        for (size_t i = mSubMeshesData.size(); i < mesh.mSubMeshes.size(); ++i) {
            mesh.mSubMeshes[i].destroy(wasUsingSharedIbo);
        }
    }
    // Allocate correct number of submeshes
    mesh.mSubMeshes.resize(mSubMeshesData.size());

    const bool usingTextureUbo = mMainSubMeshData.mTextureCount > 0;

    // Hook in shared IBOs if needed
    bool usingSharedIbo = false;
    const ui8 polyTypeBits = mPolyTypeFlags.getBits();
    if (polyTypeBits == e_cast(PolyTypeFlags::QUADS)) {
        usingSharedIbo = true;
        setSharedIbo(mesh, wasUsingSharedIbo, sQuadIbo);
    }
    else if ((polyTypeBits == e_cast(PolyTypeFlags::TERRAIN)) || (polyTypeBits == e_cast(PolyTypeFlags::WATER))) {
        usingSharedIbo = true;
        setSharedIbo(mesh, wasUsingSharedIbo, sTerrainIbo);
    }
    else {
        // Make sure we don't have terrain mixed with something else
        assert(!mPolyTypeFlags.isBitSet(PolyTypeFlags::TERRAIN));
    }
    static_assert(e_cast(PolyTypeFlags::COUNT) == 5, "Update any new shared IBO");

    // Allocate all buffers if needed
    initMeshBuffers(mesh.mMainMesh, usingTextureUbo, !usingSharedIbo);
    for (auto&& subMesh : mesh.mSubMeshes) {
        initMeshBuffers(subMesh, usingTextureUbo, !usingSharedIbo);
    }

    // Upload data
    uploadMeshData(mesh.mMainMesh, mMainSubMeshData, drawMode);
    mMainSubMeshData.clear();
    for (size_t i = 0; i < mesh.mSubMeshes.size(); ++i) {
        uploadMeshData(mesh.mSubMeshes[i], mSubMeshesData[i], drawMode);
        mSubMeshesData[i].clear();
    }

    // Cleanup
    // TODO: Do we need this really?
    mSubMeshesData.clear();
    mTextureToSubmesh.clear();
    mPolyTypeFlags.clearBits();

    glBindVertexArray(0);
}


void* MeshBuilder::operator new(size_t count) {
    assert(IS_MAIN_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void MeshBuilder::operator delete(void* pointer, size_t size) {
    assert(IS_MAIN_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}

void MeshBuilder::setSharedIbo(Mesh& mesh, const bool wasUsingSharedIbo, VGBuffer sharedIbo) {
    // Delete old IBO if needed
    if (mesh.mMainMesh.mIbo != 0 && !wasUsingSharedIbo) {
        glDeleteBuffers(1, &mesh.mMainMesh.mIbo);
    }
    mesh.mMainMesh.mIbo = sharedIbo;
    for (auto&& subMesh : mesh.mSubMeshes) {
        // Delete old IBO if needed
        if (subMesh.mIbo != 0 && !wasUsingSharedIbo) {
            glDeleteBuffers(1, &subMesh.mIbo);
        }
        subMesh.mIbo = sharedIbo;
    }
    mesh.mFlags.setBit(MeshFlags::USING_SHARED_IBO);
}

void MeshBuilder::initMeshBuffers(SubMeshData& subMesh, bool allocateUbo, bool allocateIbo) {
    // VAO
    if (subMesh.mVao == 0) {
        glGenVertexArrays(1, &subMesh.mVao);
    }
    // VBO
    glBindVertexArray(subMesh.mVao);
    if (subMesh.mVbo == 0) {
        glGenBuffers(1, &subMesh.mVbo);
    }
    // UBO
    if (allocateUbo) {
        if (subMesh.mTextureUbo == 0) {
            glGenBuffers(1, &subMesh.mTextureUbo);
        }
    }
    else if (subMesh.mTextureUbo) {
        glDeleteBuffers(1, &subMesh.mTextureUbo);
        subMesh.mTextureUbo = 0;
    }
    // IBO
    if (allocateIbo && subMesh.mIbo == 0) {
        glGenBuffers(1, &subMesh.mIbo);
        // We dont need to delete IBO if non allocating, since
        // we will have already done so in finishMesh
    }

    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    // texture UBOs go at index 1 since globalUBO is index 0
    if (subMesh.mTextureUbo) {
        glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mTextureUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mTextureUbo);
    }

    checkGlError("MeshBuilder::initMeshBuffers");
}

void MeshBuilder::uploadMeshData(SubMeshData& subMesh, const InProgressSubMeshData& data, MeshDrawMode drawMode) {
    glBindVertexArray(subMesh.mVao);
    
    const size_t vertexCount = data.mVerts.size();

    // IBO
    if (subMesh.mIbo == sQuadIbo) {
        subMesh.mIndexCount = (vertexCount / 4u) * 6u;
        assert(subMesh.mIndexCount < MAX_QUAD_MESH_INDICES);
    }
    else if (subMesh.mIbo == sTerrainIbo) {
        if (mPolyTypeFlags.isBitSet(PolyTypeFlags::WATER)) {
            subMesh.mIndexCount = WATER_MESH_INDICES;
        }
        else {
            subMesh.mIndexCount = TERRAIN_MESH_INDICES;
        }
    }
    else {
        // Non shared IBO
        // TODO: Support ui16 compression
        subMesh.mIndexCount = data.mIndices.size();
        const ui32 indexBufferSizeBytes = subMesh.mIndexCount * sizeof(ui32);
        // Allocate orphaned
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
        // Set data
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, data.mIndices.data());
    }
    const unsigned bufferSizeBytes = vertexCount * sizeof(Vertex32);

    // VBO
    // Allocate orphaned
    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, data.mVerts.data());

    // UBO
    if (subMesh.mTextureUbo) {
        const ui32 textureBufferSizeBytes = data.mTextureCount * sizeof(VGTexture);
        // Allocate orphaned
        glBufferData(GL_UNIFORM_BUFFER, textureBufferSizeBytes, nullptr, GL_STATIC_DRAW); // allocate 152 bytes of memory
        // Set data
        glBufferSubData(GL_UNIFORM_BUFFER, 0, textureBufferSizeBytes, data.mTextures);
    }
    checkGlError("MeshBuilder::uploadMeshData");

    bindVertexAttribs(subMesh);
}

void MeshBuilder::bindVertexAttribs(SubMeshData& subMesh)
{
    if (mPolyTypeFlags.isBitSet(PolyTypeFlags::TERRAIN)) {
        // Terrain verts
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
    }
    else if (mPolyTypeFlags.isBitSet(PolyTypeFlags::WATER)) {
        // Water verts
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1 /*index*/, 1 /*size*/, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, depth));
    }
    else {
        // Standard verts
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1 /*index*/, 2 /*size*/, GL_FLOAT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, uvs));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2 /*index*/, 1 /*size*/, GL_UNSIGNED_SHORT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, textureId));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, color));
        //glVertexAttribPointer(4 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, windInfluence));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4 /*index*/, 3 /*size*/, GL_BYTE, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, normal));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5 /*index*/, 2 /*size*/, GL_BYTE, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, tangent));
    }
}

void MeshBuilder::initStaticIBOs() {

    // ========================================
    // =              QUADS                   =
    // ========================================
    if (sQuadIbo) {
        return;
    }

    ui32 i = 0;
    std::vector<ui32> quadIndices(MAX_QUAD_MESH_INDICES);
    for (ui32 v = 0; i < MAX_QUAD_MESH_INDICES; v += 4u) {
        quadIndices[i++] = v;
        quadIndices[i++] = v + 1;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v;
    }

    glGenBuffers(1, &sQuadIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sQuadIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_QUAD_MESH_INDICES * sizeof(ui32), quadIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // ========================================
    // =              TERRAIN                 =
    // ========================================
    std::vector<ui32> indices(TERRAIN_MESH_INDICES);
    ui32 index = 0;
    for (int y = 0; y < TERRAIN_MESH_WIDTH_QUADS; y++) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_QUADS; x++) {
            // Compute index of back left vertex
            ui32 vertIndex = y * TERRAIN_MESH_WIDTH_VERTS + x;
            // Change triangle orientation based on odd or even
            if ((x + y) % 2) {
                indices[index++] = vertIndex;
                indices[index++] = vertIndex + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex;
            }
            else {
                indices[index++] = vertIndex + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex;
                indices[index++] = vertIndex + 1;
            }
        }
    }

    // Skirt vertices
    ui32 skirtIndex = TERRAIN_MESH_WIDTH_VERTS_SQ;
    ui32 vertIndex;
    // Top Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = i;
        indices[index++] = skirtIndex;
        indices[index++] = skirtIndex + 1;
        indices[index++] = vertIndex + 1;
        indices[index++] = vertIndex + 1;
        indices[index++] = vertIndex;
        indices[index++] = skirtIndex;
        skirtIndex++;
    }
    skirtIndex++; // Skip last vertex
    // Left Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = i * TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = skirtIndex;
        indices[index++] = vertIndex;
        indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex;
        skirtIndex++;
    }
    skirtIndex++; // Skip last vertex
    // Right Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = i * TERRAIN_MESH_WIDTH_VERTS + TERRAIN_MESH_WIDTH_VERTS - 1;
        indices[index++] = vertIndex;
        indices[index++] = skirtIndex;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex + 1;
        indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
        indices[index++] = vertIndex;
        skirtIndex++;
    }
    skirtIndex++;
    // Bottom Skirt
    for (ui32 i = 0; i < TERRAIN_MESH_WIDTH_QUADS; i++) {
        vertIndex = TERRAIN_MESH_WIDTH_VERTS_SQ - TERRAIN_MESH_WIDTH_VERTS + i;
        indices[index++] = vertIndex;
        indices[index++] = vertIndex + 1;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex + 1;
        indices[index++] = skirtIndex;
        indices[index++] = vertIndex;
        skirtIndex++;
    }

    assert(index == TERRAIN_MESH_INDICES);

    glGenBuffers(1, &sTerrainIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sTerrainIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, TERRAIN_MESH_INDICES * sizeof(ui32), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    checkGlError("TerrainMesh::initGlobalIBO");
}
