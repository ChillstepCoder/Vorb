#pragma once

#include "Mesh.h"
#include "Vertex.h"

struct SubMeshBufferData {
    void clear() {
        mVerts.clear();
        mIndices.clear();
        mTextures.clear();
    }

    // TODO: Pool allocators or reserve?
    std::vector<Vertex32> mVerts;
    std::vector<ui32> mIndices;
    std::vector<TextureHandle> mTextures;
};

// Static common utils
class MeshBuilderCommon
{
public:
    static void initMeshBuffers(SubMeshData& subMesh, bool allocateIbo);
    static void uploadMeshData(SubMeshData& subMesh, const f32v3& position, const SubMeshBufferData& data, MeshDrawMode drawMode);
};

