#include "stdafx.h"
#include "PhysicsDebugRenderer.h"


#ifdef JPH_DEBUG_RENDERER

#include "debugging/DebugRenderer.h"

#include "rendering/MaterialShaderRepository.h"
#include "camera/Camera3D.h"

#include <Vorb/graphics/ShaderManager.h>

constexpr GLuint DEBUG_MODEL_COLORS_BINDING_POINT = 3;

namespace {
    static const cString DEBUG_VERT_SRC = R"(
// Uniforms
uniform mat4 unVP;
uniform vec3 unCameraPos;

// Input
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 3) in vec4 vColor;
layout(location = 4) in mat4 vModelMatrix;
layout(location = 8) in vec4 vModelColor;

// Output to fragment shader
out vec4 fColor;
out vec3 fNormal;
out vec3 fWorldPos;

void main() {
    vec4 worldPos = vModelMatrix * vPosition;
    fWorldPos = worldPos.xyz;
    fNormal = mat3(vModelMatrix) * vNormal;  // Transform normal to world space
    fColor = vColor * vModelColor;
    
    worldPos.xyz -= unCameraPos;
    gl_Position = unVP * worldPos;
}
)";

    static const cString DEBUG_FRAG_SRC = R"(
// Uniforms
uniform vec3 unLightDir = vec3(-1.0, -1.0, -1.0);

// Input from vertex shader
in vec4 fColor;
in vec3 fNormal;
in vec3 fWorldPos;
uniform float unAlpha = 1.0;

// Output
out vec4 pColor;

void main() {
    vec3 normal = normalize(fNormal);
    vec3 lightDir = normalize(-unLightDir);
    
    // Ambient light
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * fColor.rgb;
    
    // Diffuse light
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * fColor.rgb;
    
    // Combine lighting
    vec3 result = ambient + diffuse;
    pColor = vec4(result, fColor.a * unAlpha);
}
)";
}

JPH::RVec3 rVecFromF32v3(f32v3 v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

JPH::Vec3 fVecFromF32v3(f32v3 v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

using DebugVertex = JPH::DebugRenderer::Vertex;

class DebugTriangleBatch : public JPH::RefTargetVirtual {
public:

    DebugTriangleBatch(const JPH::DebugRenderer::Triangle* triangles, int numTriangles) {
        glCreateVertexArrays(1, &vao);
        vbo.allocate(numTriangles * sizeof(JPH::DebugRenderer::Triangle), triangles, 0);
        glVertexArrayVertexBuffer(vao, 0, vbo.getHandle(), 0, sizeof(DebugVertex));
        drawCount = numTriangles * 3;
        checkGlError("DebugTriangleBatch::DebugTriangleBatch 1");
        bindAttribs();
    }

    DebugTriangleBatch(
        const DebugVertex* vertices, int numVertices, const JPH::uint32* indices, int numIndices
    ) {
        glCreateVertexArrays(1, &vao);
        vbo.allocate(numVertices * sizeof(DebugVertex), vertices, 0);
        ibo.allocate(numIndices * sizeof(JPH::uint32), indices, 0);
        glVertexArrayVertexBuffer(vao, 0, vbo.getHandle(), 0, sizeof(DebugVertex));
        glVertexArrayElementBuffer(vao, ibo.getHandle());
        usesIndices = true;
        drawCount = numIndices;
        checkGlError("DebugTriangleBatch::DebugTriangleBatch 2");
        bindAttribs();
    }

    virtual void AddRef() override { ++mRefCount; }
    virtual void Release() override { if (--mRefCount == 0) delete this; }

    void draw() {
        glBindVertexArray(vao);
        if (usesIndices) {
            glDrawElements(GL_TRIANGLES, drawCount, GL_UNSIGNED_INT, 0);
        }
        else {
            glDrawArrays(GL_TRIANGLES, 0, drawCount);
        }
    }

    void drawInstanced(i32 instanceCount) {
        glBindVertexArray(vao);
        if (usesIndices) {
            glDrawElementsInstanced(GL_TRIANGLES, drawCount, GL_UNSIGNED_INT, 0, instanceCount);
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, drawCount, instanceCount);
        }
    }

    VGVertexArray getVao() const { return vao; }

private:
    void bindAttribs() {
        ASSERT_RENDER_THREAD();
        glEnableVertexArrayAttrib(vao, 0);
        glVertexArrayAttribFormat(
            vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, GL_FALSE, offsetof(DebugVertex, mPosition)
        );
        glVertexArrayAttribBinding(vao, 0, 0);

        glEnableVertexArrayAttrib(vao, 1);
        glVertexArrayAttribFormat(
            vao, 1 /*index*/, 3 /*size*/, GL_FLOAT, GL_TRUE, offsetof(DebugVertex, mNormal)
        );
        glVertexArrayAttribBinding(vao, 1, 0);

        glEnableVertexArrayAttrib(vao, 2);
        glVertexArrayAttribFormat(
            vao, 2 /*index*/, 2 /*size*/, GL_FLOAT, GL_TRUE, offsetof(DebugVertex, mUV)
        );
        glVertexArrayAttribBinding(vao, 2, 0);

        glEnableVertexArrayAttrib(vao, 3);
        glVertexArrayAttribFormat(
            vao, 3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(DebugVertex, mColor)
        );
        glVertexArrayAttribBinding(vao, 3, 0);

        // Transforms
        glEnableVertexArrayAttrib(vao, 4);
        glEnableVertexArrayAttrib(vao, 5);
        glEnableVertexArrayAttrib(vao, 6);
        glEnableVertexArrayAttrib(vao, 7);
        glVertexArrayAttribFormat(vao, 4, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(vao, 5, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4));
        glVertexArrayAttribFormat(vao, 6, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 2);
        glVertexArrayAttribFormat(vao, 7, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 3);
        glVertexArrayAttribBinding(vao, 4, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(vao, 5, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(vao, 6, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(vao, 7, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayBindingDivisor(vao, MODEL_TRANSFORMS_BINDING_POINT, 1);

        // Model colors
        glEnableVertexArrayAttrib(vao, 8);
        glVertexArrayAttribFormat(vao, 8, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0);
        glVertexArrayAttribBinding(vao, 8, DEBUG_MODEL_COLORS_BINDING_POINT);
        glVertexArrayBindingDivisor(vao, DEBUG_MODEL_COLORS_BINDING_POINT, 1);

        checkGlError("DebugTriangleBatch::bindAttribs");
    }

    GLBuffer vbo;
    GLBuffer ibo;
    VGVertexArray vao;
    i32 drawCount = 0;
    bool usesIndices = false;
    std::atomic_int mRefCount = 0;
};

PhysicsDebugRenderer::PhysicsDebugRenderer() : JPH::DebugRenderer()
{
    Initialize();
    mProgram = std::make_unique<vg::GLProgram>();
    *mProgram = vg::ShaderManager::createProgram(DEBUG_VERT_SRC, DEBUG_FRAG_SRC, nullptr);
    checkGlError("PhysicsDebugRenderer::PhysicsDebugRenderer");
}

void PhysicsDebugRenderer::PrepareFrame(const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    vg::sBlendStates.ALPHA.set();
    mCamera = &camera;

    mProgram->use();
    glUniformMatrix4fv(mProgram->getUniform("unVP"), 1, GL_FALSE, &mCamera->getVPMatrix()[0][0]);
    glUniform3fv(mProgram->getUniform("unCameraPos"), 1, &mCamera->getPosition()[0]);

    checkGlError("PhysicsDebugRenderer::PrepareFrame");
}

void PhysicsDebugRenderer::PreDraw(bool isStatic) {
    if (isStatic) {
        mInstanceArrayUpdateIndex = 1;
        // Clear static for new gather
        for (InstanceMap* primitiveMap : {
           &mInstanceMaps[1].instancesWireframe, &mInstanceMaps[1].instancesBackFacing, &mInstanceMaps[1].instances,
        }) {
            for (InstanceMap::value_type& v : *primitiveMap) {
                v.second.mInstances.clear();
            }
        }
    }
    else {
        mInstanceArrayUpdateIndex = 0;
    }
}

void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) {
    ASSERT_RENDER_THREAD();
    AM::DebugRenderer::drawLineBetweenPoints(
        f32v3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()),
        f32v3(inTo.GetX(), inTo.GetY(), inTo.GetZ()),
        color4(inColor.r, inColor.g, inColor.b, inColor.a)
    );
}

void PhysicsDebugRenderer::DrawTriangle(
    JPH::RVec3Arg inV1,
    JPH::RVec3Arg inV2,
    JPH::RVec3Arg inV3,
    JPH::ColorArg inColor,
    ECastShadow inCastShadow /*= ECastShadow::Off*/
) {
    ASSERT_RENDER_THREAD();
    throw std::logic_error("The method or operation is not implemented.");
}

JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(
    const Triangle* inTriangles, int inTriangleCount
) {
    ASSERT_RENDER_THREAD();
    DebugTriangleBatch* data = new DebugTriangleBatch(inTriangles, inTriangleCount);
    return data;
}

JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(
    const Vertex* inVertices, int inVertexCount, const ui32* inIndices, int inIndexCount
) {
    ASSERT_RENDER_THREAD();
    DebugTriangleBatch* data = new DebugTriangleBatch(inVertices, inVertexCount, inIndices, inIndexCount);
    return data;
}

void PhysicsDebugRenderer::DrawGeometry(
    JPH::RMat44Arg inModelMatrix,
    const JPH::AABox& inWorldSpaceBounds,
    float inLODScaleSq,
    JPH::ColorArg inModelColor,
    const GeometryRef& inGeometry,
    ECullMode inCullMode /*= ECullMode::CullBackFace*/,
    ECastShadow inCastShadow /*= ECastShadow::On*/,
    EDrawMode inDrawMode /*= EDrawMode::Solid */
) {
    ASSERT_RENDER_THREAD();

    JPH::Mat44 modelMatrix = inModelMatrix.ToMat44();

    UNUSED(inCastShadow);

    InstanceMaps& instances = mInstanceMaps[mInstanceArrayUpdateIndex];

    if (inDrawMode == EDrawMode::Wireframe) {
        instances.instancesWireframe[inGeometry].mInstances.push_back({ modelMatrix, inModelColor, inWorldSpaceBounds, inLODScaleSq });
    }
    else
    {
        if (inCullMode != ECullMode::CullFrontFace) {
            instances.instances[inGeometry].mInstances.push_back({ modelMatrix, inModelColor, inWorldSpaceBounds, inLODScaleSq });
        }

        if (inCullMode != ECullMode::CullBackFace) {
            instances.instancesBackFacing[inGeometry].mInstances.push_back({ modelMatrix, inModelColor, inWorldSpaceBounds, inLODScaleSq });
        }
    }
}

void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor /*= JPH::Color::sWhite*/, float inHeight /*= 0.5f*/) {
    ASSERT_RENDER_THREAD();
    //throw std::logic_error("The method or operation is not implemented.");
}

void PhysicsDebugRenderer::EndFrame() {
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();

    glUniform1f(mProgram->getUniform("unAlpha"), mRenderSettings.alpha);

    size_t mapCount = 0;
    // 0 = dynamic 1 = static
    for (InstanceMap* primitiveMap : {
         &mInstanceMaps[0].instancesWireframe,  &mInstanceMaps[1].instancesWireframe,  // Wireframe 0,1
         &mInstanceMaps[0].instancesBackFacing, &mInstanceMaps[1].instancesBackFacing, // Backface 2,3
         &mInstanceMaps[0].instances,           &mInstanceMaps[1].instances,           // Solid standard 4,5
    }) {
        if (mapCount < 2) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else if (mapCount < 4) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glCullFace(GL_FRONT);
        }
        else {
            glCullFace(GL_BACK);
        }
        for (InstanceMap::value_type& v : *primitiveMap)
        {
            // TODO: Use inLODScaleSq
            const JPH::Array<LOD>& geometryLods = v.first->mLODs;

            // Iterate over all instances
            std::vector<InstanceWithLODInfo>& instances = v.second.mInstances;

            assert(geometryLods.size() <= MAX_PHYSICS_DEBUG_LODS);
            // Static to prevent realloc
            static std::vector<JPH::Mat44> lodMatrices[MAX_PHYSICS_DEBUG_LODS];
            static std::vector<JPH::Color> lodColors[MAX_PHYSICS_DEBUG_LODS];
            
            // Streaming buffers
            GLBuffer mLodMatrixBuffers[MAX_PHYSICS_DEBUG_LODS];
            GLBuffer mLodColorBuffers[MAX_PHYSICS_DEBUG_LODS];

            for (size_t i = 0; i < instances.size(); ++i) {
                const InstanceWithLODInfo& srcInstance = instances[i];

                const LOD& lod = v.first->GetLOD(fVecFromF32v3(mCamera->getPosition()), srcInstance.mWorldSpaceBounds, srcInstance.mLODScaleSq);
                size_t lodIndex = &lod - geometryLods.data();

                const JPH::Vec3 translation = srcInstance.mModelMatrix.GetTranslation();
                const f32v3 pos(translation.GetX(), translation.GetY(), translation.GetZ());
                // Inaccurate camera culling
                if (mCamera->getFrustum().sphereInFrustum(pos - mCamera->getPosition(), srcInstance.mWorldSpaceBounds.GetExtent().GetX() * 2.0f)) {
                    lodMatrices[lodIndex].push_back(srcInstance.mModelMatrix);
                    lodColors[lodIndex].push_back(srcInstance.mModelColor);
                }
            }

            // Instanced draw
            for (size_t i = 0; i < geometryLods.size(); ++i) {
                if (lodMatrices[i].empty()) {
                    continue;
                }

                mLodMatrixBuffers[i].allocate(lodMatrices[i].size() * sizeof(JPH::Mat44), lodMatrices[i].data(), 0);
                mLodColorBuffers[i].allocate(lodColors[i].size() * sizeof(JPH::Color), lodColors[i].data(), 0);

                DebugTriangleBatch* batch = static_cast<DebugTriangleBatch*>(geometryLods[i].mTriangleBatch.GetPtr());

                static_assert(sizeof(JPH::Mat44) == sizeof(f32m4));
                mLodMatrixBuffers[i].bindAsVertexArrayVertexBuffer(batch->getVao(), MODEL_TRANSFORMS_BINDING_POINT, 0, sizeof(f32m4));
                mLodColorBuffers[i].bindAsVertexArrayVertexBuffer(batch->getVao(), DEBUG_MODEL_COLORS_BINDING_POINT, 0, sizeof(color4));

                batch->drawInstanced(lodMatrices[i].size());
            }

            // Always clear instances for dynamic
            if (mapCount % 2 == 0) {
                instances.clear();
            }

            // Free lod data
            for (size_t i = 0; i < MAX_PHYSICS_DEBUG_LODS; ++i) {
                lodMatrices[i].clear();
                lodColors[i].clear();
            }
        }
        ++mapCount;
    }

    checkGlError("PhysicsDebugRenderer::DrawGeometry");
}

#endif // JPH_DEBUG_RENDERER