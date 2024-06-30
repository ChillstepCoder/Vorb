#include "stdafx.h"
#include "PhysicsDebugRenderer.h"


#ifdef JPH_DEBUG_RENDERER

#include "debugging/DebugRenderer.h"

#include "rendering/MaterialShaderRepository.h"
#include "camera/Camera3D.h"

#include <Vorb/graphics/ShaderManager.h>

namespace {
    static const cString DEBUG_VERT_SRC = R"(
// Uniforms
uniform mat4 unVP;
uniform mat4 unM;
uniform vec3 unCameraPos;
uniform vec4 unModelColor;

// Input
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 3) in vec4 vColor;

// Output to fragment shader
out vec4 fColor;
out vec3 fNormal;
out vec3 fWorldPos;

void main() {
    vec4 worldPos = unM * vPosition;
    fWorldPos = worldPos.xyz;
    fNormal = mat3(unM) * vNormal;  // Transform normal to world space
    fColor = vColor * unModelColor;
    
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
    pColor = vec4(result, fColor.a);
}
)";
}

JPH::RVec3 rVecFromF32v3(f32v3 v) {
    return JPH::RVec3(v.x, v.y, v.z);
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

size_t selectLOD(f32 distanceSq, size_t maxLOD) {
    if (distanceSq < SQ(48.f)) {
        return 0;
    }
    else if (distanceSq < SQ(72.f)) {
        return std::min(maxLOD - 1, (size_t)1);
    } else if (distanceSq < SQ(128.f)) {
        return std::min(maxLOD - 1, (size_t)2);
    } else if (distanceSq < SQ(164.f)) {
        return std::min(maxLOD - 1, (size_t)3);
    }
    else {
        return maxLOD - 1;
    }
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
)
{
    ASSERT_RENDER_THREAD();
    UNUSED(inCastShadow, inCullMode, inWorldSpaceBounds);

    const JPH::Array<LOD>& geometryLods = inGeometry->mLODs;

    const JPH::DVec3 translation = inModelMatrix.GetTranslation();
    const f32v3 pos(translation.GetX(), translation.GetY(), translation.GetZ());
    const size_t lod = selectLOD(glm::length2(pos - mCamera->getPosition()), geometryLods.size());
    DebugTriangleBatch* batch = static_cast<DebugTriangleBatch*>(geometryLods[lod].mTriangleBatch.GetPtr());

    const JPH::Mat44 modelMatrix = inModelMatrix.ToMat44();

    glUniform4f(mProgram->getUniform("unModelColor"),
        inModelColor.r / 255.f, inModelColor.g / 255.f, inModelColor.b / 255.f, inModelColor.a / 255.f);
    glUniformMatrix4fv(mProgram->getUniform("unM"), 1, GL_FALSE, (f32*)&modelMatrix);
    static_assert(sizeof(JPH::Mat44) == sizeof(f32m4));

    if (inDrawMode == JPH::DebugRenderer::EDrawMode::Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    batch->draw();

    if (inDrawMode == JPH::DebugRenderer::EDrawMode::Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    checkGlError("PhysicsDebugRenderer::DrawGeometry");
}

void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor /*= JPH::Color::sWhite*/, float inHeight /*= 0.5f*/)
{
    ASSERT_RENDER_THREAD();
    //throw std::logic_error("The method or operation is not implemented.");
}

#endif // JPH_DEBUG_RENDERER