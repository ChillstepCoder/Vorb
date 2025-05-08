#pragma once

class MeshCpuData;
class FBXRawModel;

namespace MeshOperations {
    void rotate90AboutAxis(MeshCpuData& mesh, const f32v3& axis);
    void applyScale(MeshCpuData& mesh, f32 scale);
    void setAllNormals(FBXRawModel& rawMesh, const f32v3& normal, const f32v3& tangent);
};

