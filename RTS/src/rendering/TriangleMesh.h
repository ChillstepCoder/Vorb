#pragma once

#include <Vorb/graphics/gtypes.h>
#include "MeshBase.h"

#include "rendering/TileVertex.h"

struct aiFace;

template <typename VERTEX>
class ITriangleMesh : public MeshBase {
public:
    ITriangleMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(ITriangleMesh);

    void setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode);
};

class TriangleMesh : public ITriangleMesh<TriangleVertex> {
public:
    TriangleMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(TriangleMesh);

    //void init() override; // TODO: We cant override MeshBase::init because its called from constructor and that is illegal

    void reserveTriangleCount(size_t count);
    void addTriangle(TriangleVertex verts[3]);
    void draw(const vg::GLProgram& program) const override;
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;                

    std::vector<TriangleVertex> mVertexData; // TODO: Recycle?
};

class IndexedTriangleMesh : public ITriangleMesh<ModelVertex> {
public:
    IndexedTriangleMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(IndexedTriangleMesh);

    void setFaces(const aiFace* faces, ui32 numFaces);
    void draw(const vg::GLProgram& program) const override;
    void finishMesh(MeshDrawMode drawMode) override;

    void setDiffuseTexture(VGTexture texture) { mDiffuseTexture = texture; }
    void setNormalTexture(VGTexture texture) { mNormalTexture = texture; }
    void setSpecularTexture(VGTexture texture) { mSpecularTexture = texture; }
    VGTexture getDiffuseTexture() const { return mDiffuseTexture; }
    VGTexture getNormalTexture() const { return mNormalTexture; }
    VGTexture getSpecularTexture() const { return mSpecularTexture; }

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    VGIndexBuffer mIbo = 0;
    VGTexture mDiffuseTexture = 0;
    VGTexture mNormalTexture = 0;
    VGTexture mSpecularTexture = 0;
};

// Templated Mesh implementation
template <typename VERTEX>
void ITriangleMesh<VERTEX>::setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode) {

    lazyInitBuffers();
    mIndexCount = vertexCount;
    const unsigned bufferSizeBytes = vertexCount * sizeof(VERTEX);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

