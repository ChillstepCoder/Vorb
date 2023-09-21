#pragma once

class MeshCpuData;
class FBXRawMesh;

namespace MeshOperations {
    void rotate90AboutAxis(MeshCpuData& mesh, const f32v3& axis);
    void applyScale(MeshCpuData& mesh, f32 scale);
    void setAllNormals(FBXRawMesh& rawMesh, const f32v3& normal, const f32v3& tangent);
};

