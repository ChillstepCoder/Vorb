#pragma once

#include <Vorb/graphics/gtypes.h>

DECL_VG(class GLProgram);

class ChunkMesh {
public:
    ChunkMesh();

    class Batch {
    public:
        void set(ui32 iOff, ui32 texID);
        ui32 textureID;
        ui32 indices;
        ui32 indexOffset;
    };

    VGVertexArray m_vao = 0; ///< Vertex Array Object
    VGBuffer m_vbo = 0; ///< Vertex Buffer Object
    VGBuffer m_ibo = 0; ///< Index Buffer Object
    ui32 m_indexCapacity = 0; ///< Current capacity of the m_ibo
    std::vector<Batch> m_batches; ///< Vector of batches for rendering

    static vg::GLProgram s_program; ///< Shader handle
};

