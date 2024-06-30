#pragma once

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>

DECL_VG(class GLProgram);

class Camera3D;

class PhysicsDebugRenderer : public JPH::DebugRenderer {
public:
    PhysicsDebugRenderer();

    void PrepareFrame(const Camera3D& camera);

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;

    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;

    Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;

    Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const ui32* inIndices, int inIndexCount) override;

    void DrawGeometry(
        JPH::RMat44Arg inModelMatrix,
        const JPH::AABox& inWorldSpaceBounds,
        float inLODScaleSq,
        JPH::ColorArg inModelColor,
        const GeometryRef& inGeometry,
        ECullMode inCullMode = ECullMode::CullBackFace,
        ECastShadow inCastShadow = ECastShadow::On,
        EDrawMode inDrawMode = EDrawMode::Solid
    ) override;

    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override;

private:
    const Camera3D* mCamera = nullptr;
    std::unique_ptr<vg::GLProgram> mProgram;
};

static std::unique_ptr<PhysicsDebugRenderer> sDebugRenderer;

#endif // JPH_DEBUG_RENDERER