#pragma once

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Renderer/DebugRenderer.h>

class MaterialShaderDef;

class DynamicBodyDrawFilter : public JPH::BodyDrawFilter {
public:
    bool ShouldDraw(const JPH::Body& inBody) const {
        return inBody.GetMotionType() == JPH::EMotionType::Dynamic;
    }
};

class StaticBodyDrawFilter : public JPH::BodyDrawFilter {
public:
    bool ShouldDraw(const JPH::Body& inBody) const {
        return inBody.GetMotionType() == JPH::EMotionType::Static;
    }
};

class Camera3D;

struct GeometryFrameData {
    JPH::Mat44 mModelMatrix;
    JPH::Color mModelColor;
};

class GeometryHash {
public:
    size_t operator()(const JPH::DebugRenderer::GeometryRef& v) const {
        return boost::hash<JPH::DebugRenderer::Geometry*>()(v.GetPtr());
    }
};

class PhysicsDebugRenderer : public JPH::DebugRenderer {
public:
    PhysicsDebugRenderer();
    ~PhysicsDebugRenderer();

    void PrepareFrame(const Camera3D& camera);

    void PreDraw(bool isStatic);

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

    void EndFrame();

    struct RenderSettings {
        bool enableDebugDraw = false;
        f32 alpha = 0.7f;
        i32 queryRenderTime = 32;
        bool showRaycasts = false;
        bool showShapeQueries = false;
        JPH::BodyManager::DrawSettings bodyDrawSettings;
    } mRenderSettings;

private:

    static constexpr i32 MAX_PHYSICS_DEBUG_LODS = 4;


    /// Properties for a single rendered instance
    struct Instance {
        /// Constructor
        Instance(JPH::Mat44Arg inModelMatrix, JPH::ColorArg inModelColor) : mModelMatrix(inModelMatrix), mModelColor(inModelColor) { }

        JPH::Mat44 mModelMatrix;
        JPH::Color mModelColor;
    };

    /// Rendered instance with added information for lodding
    struct InstanceWithLODInfo : public Instance {
        /// Constructor
        InstanceWithLODInfo(JPH::Mat44Arg inModelMatrix, JPH::ColorArg inModelColor, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq) : Instance(inModelMatrix, inModelColor), mWorldSpaceBounds(inWorldSpaceBounds), mLODScaleSq(inLODScaleSq) { }

        /// Bounding box for culling
        JPH::AABox mWorldSpaceBounds;

        /// Square of scale factor for LODding (1 = original, > 1 = lod out further, < 1 = lod out earlier)
        float mLODScaleSq;
    };

    /// Properties for a batch of instances that have the same primitive
    struct Instances {
        std::vector<InstanceWithLODInfo> mInstances;
    };
    using InstanceMap = UnorderedFlatMap<GeometryRef, Instances, GeometryHash>;

    struct InstanceMaps {
        InstanceMap	instancesWireframe;
        InstanceMap	instancesBackFacing;
        InstanceMap	instances;
    } mInstanceMaps[2]; // 0 = Dynamic 1 = Static

    const Camera3D* mCamera = nullptr;
    AssetHandlePtr<MaterialShaderDef> mShaderHandle;
    const MaterialShaderDef* mShader;

    i32 mInstanceArrayUpdateIndex = 0;
};

extern std::unique_ptr<PhysicsDebugRenderer> sPhysicsDebugRenderer;

#endif // JPH_DEBUG_RENDERER