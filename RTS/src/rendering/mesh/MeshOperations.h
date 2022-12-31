#pragma once

class MeshCpuData;

namespace MeshOperations {
    void rotate90AboutAxis(MeshCpuData& mesh, const f32v3& axis);
    void applyScale(MeshCpuData& mesh, f32 scale);
};

