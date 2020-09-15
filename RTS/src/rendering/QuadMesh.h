#pragma once

#include <Vorb/graphics/gtypes.h>

DECL_VG(class GLProgram);
class Camera2D;
struct BasicVertex;

class QuadMesh {
public:
    QuadMesh();
    ~QuadMesh();

    /*class Batch {
    public:
        void set(ui32 iOff, ui32 texID);
        ui32 textureID;
        ui32 indices;
        ui32 indexOffset;
    };*/

    // Indices are 0, 2, 3, 3, 1, 0
    void setData(const BasicVertex* meshData, int vertexCount, VGTexture texture);
    void draw(const Camera2D& camera, vg::GLProgram& program);

private:
    VGVertexArray mVao = 0; ///< Vertex Array Object
    VGBuffer mVbo = 0; ///< Vertex Buffer Object
    VGBuffer mIbo = 0; ///< Index Buffer Object
    VGTexture mTexture = 0;
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    const vg::GLProgram* mLastUsedProgram = nullptr;
   // std::vector<Batch> mBatches; ///< Vector of batches for rendering
};
