#pragma once

#include <Vorb/graphics/gtypes.h>

DECL_VG(class GLProgram);
class Camera2D;
struct ChunkVertex;

class ChunkMesh {
public:
    ChunkMesh();
    ~ChunkMesh();

    /*class Batch {
    public:
        void set(ui32 iOff, ui32 texID);
        ui32 textureID;
        ui32 indices;
        ui32 indexOffset;
    };*/

    void setData(const ChunkVertex* meshData, int vertexCount, const ui32* indexData, VGTexture texture);
    void draw(const Camera2D& camera);

    VGVertexArray mVao = 0; ///< Vertex Array Object
    VGBuffer mVbo = 0; ///< Vertex Buffer Object
    VGBuffer mIbo = 0; ///< Index Buffer Object
    VGTexture mTexture = 0;
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
   // std::vector<Batch> mBatches; ///< Vector of batches for rendering

    static vg::GLProgram sProgram; ///< Shader handle
};

