#pragma once

#include "rendering/mesh/Mesh.h"

class GLIndirectBuffer;

namespace MeshDrawer {
    // TODO: Have each renderer implement its own "draw" method
    //   Renderer knows what type of mesh this is  so we can avoid
    //   branching and assert on internal state such as ubo
    void draw(const MeshGpuData& meshData);
    void draw(const MeshGpuData& meshData, MeshLODLevel lod);
    void drawInstanced(const MeshGpuData& meshData, GLsizei instanceCount);
    void drawInstanced(const MeshGpuData& meshData, MeshLODLevel lod, GLsizei instanceCount);
    void drawIndirect(const MeshGpuData& meshData, size_t numDrawCommands, const GLIndirectBuffer* buffer);
    void drawMinimum(const MeshMinimumRenderData& meshData);
};

