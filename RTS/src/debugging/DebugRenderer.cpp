#include "stdafx.h"
#include "debugging/DebugRenderer.h"

#include <Vorb/MeshGenerators.h>
#include <Vorb/graphics/RasterizerState.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/BlendState.h>
#include <glm/gtx/rotate_vector.hpp>
#include "rendering/RenderStats.h"
#include "tile/TileHandle.h"

#include "debugging/DebugMesh.h"

#include "rendering/gl/GL.h"

// For shared ibo, maybe not the place
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"

void bindSimpleMeshVertexAttribs(VGVertexArray vao) {
    GL.glEnableVertexArrayAttrib(vao, 0);
    GL.glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, GL_FALSE, offsetof(SimpleMeshVertex, position));
    GL.glVertexArrayAttribBinding(vao, 0, 0);
    GL.glEnableVertexArrayAttrib(vao, 1);
    GL.glVertexArrayAttribFormat(vao, 1 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(SimpleMeshVertex, color));
    GL.glVertexArrayAttribBinding(vao, 1, 0);
}

void bindCircleMeshVertexAttribs(VGVertexArray vao) {
    GL.glEnableVertexArrayAttrib(vao, 0);
    GL.glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, GL_FALSE, offsetof(SimpleMeshCircleVertex, position));
    GL.glVertexArrayAttribBinding(vao, 0, 0);
    GL.glEnableVertexArrayAttrib(vao, 1);
    GL.glVertexArrayAttribFormat(vao, 1 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(SimpleMeshCircleVertex, color));
    GL.glVertexArrayAttribBinding(vao, 1, 0);
    GL.glEnableVertexArrayAttrib(vao, 2);
    GL.glVertexArrayAttribFormat(vao, 2 /*index*/, 1 /*size*/, GL_FLOAT, GL_FALSE, offsetof(SimpleMeshCircleVertex, radius));
    GL.glVertexArrayAttribBinding(vao, 2, 0);
    GL.glEnableVertexArrayAttrib(vao, 3);
    GL.glVertexArrayAttribFormat(vao, 3 /*index*/, 2 /*size*/, GL_FLOAT, GL_FALSE, offsetof(SimpleMeshCircleVertex, offset));
    GL.glVertexArrayAttribBinding(vao, 3, 0);
}


struct IntPairHasher
{
    std::size_t operator()(const std::pair<i32, i32>& k) const {
        return std::hash<i32>()(k.first) ^ std::hash<i32>()(k.second);
    }
};

std::vector<SimpleMesh> sDebugMeshes;
std::vector<SimpleMesh> sDebugCircleMeshes;
std::unordered_map<std::pair<i32, i32> /*lifetime,id*/, std::vector<DebugLine>, IntPairHasher> sNewLines;
std::unordered_map<std::pair<i32, i32> /*lifetime,id*/, std::vector<DebugQuad>, IntPairHasher> sNewQuads;
std::unordered_map<std::pair<i32, i32> /*lifetime,id*/, std::vector<DebugCircle>, IntPairHasher> sNewCircles;

std::mutex sNewLinesThreadSafeMutex;
std::unordered_map<std::pair<i32, i32> /*lifetime,id*/, std::vector<DebugLine>, IntPairHasher> sNewLinesThreadSafe;


const float rotVal = glm::radians(30.0f);
void DebugRenderer::drawVector(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    ASSERT_RENDER_THREAD();
    const f32v2 end = origin + vec;
    const f32v2 tipRay = -vec * 0.2f;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
    lines.emplace_back(end, end + glm::rotate(tipRay, rotVal), color);
    lines.emplace_back(end, end + glm::rotate(tipRay, -rotVal), color);
}

void DebugRenderer::drawVector(const f32v3& origin, const f32v3& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    ASSERT_RENDER_THREAD();
    const f32v3 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    ASSERT_RENDER_THREAD();
	const f32v2 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLine(const f32v3& origin, const f32v3& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    ASSERT_RENDER_THREAD();
    const f32v3 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPoints(const f32v2& origin, const f32v2& end, color4 color, int lifeTime/* = 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPoints(const f32v3& origin, const f32v3& end, const color4& color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPointsThreadSafe(const f32v3& origin, const f32v3& end, const color4& color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(!IS_RENDER_THREAD());
    std::lock_guard<std::mutex> lockGuard(sNewLinesThreadSafeMutex);
    auto&& lines = sNewLinesThreadSafe[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawWireQuadThreadSafe(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(!IS_RENDER_THREAD());
    const f32v3 topRight = origin + f32v3(dims.x, dims.y, 0.0f);
    std::lock_guard<std::mutex> lockGuard(sNewLinesThreadSafeMutex);
    auto&& lines = sNewLinesThreadSafe[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, origin + f32v3(dims.x, 0.0f, 0.0f), color);
    lines.emplace_back(origin, origin + f32v3(0.0f, dims.y, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v3(dims.x, 0.0f, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v3(0.0f, dims.x, 0.0f), color);
}

void DebugRenderer::drawAABBThreadSafe(const f32AABB3& aabb, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    // Bottom 4
    const f32v3 v0(aabb.x, aabb.y, aabb.z);
    const f32v3 v1(aabb.x + aabb.dims.x, aabb.y, aabb.z);
    const f32v3 v2(aabb.x + aabb.dims.x, aabb.y + aabb.dims.y, aabb.z);
    const f32v3 v3(aabb.x, aabb.y + aabb.dims.y, aabb.z);
    // Top 4
    const f32v3 v4(aabb.x, aabb.y, aabb.z + aabb.dims.z);
    const f32v3 v5(aabb.x + aabb.dims.x, aabb.y, aabb.z + aabb.dims.z);
    const f32v3 v6(aabb.x + aabb.dims.x, aabb.y + aabb.dims.y, aabb.z + aabb.dims.z);
    const f32v3 v7(aabb.x, aabb.y + aabb.dims.y, aabb.z + aabb.dims.z);
    // Bottom
    std::lock_guard<std::mutex> lockGuard(sNewLinesThreadSafeMutex);
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.reserve(lines.size() + 12);
    lines.emplace_back(v0, v1, color);
    lines.emplace_back(v1, v2, color);
    lines.emplace_back(v2, v3, color);
    lines.emplace_back(v3, v0, color);
    // Top
    lines.emplace_back(v4, v5, color);
    lines.emplace_back(v5, v6, color);
    lines.emplace_back(v6, v7, color);
    lines.emplace_back(v7, v4, color);
    // Middle
    lines.emplace_back(v0, v4, color);
    lines.emplace_back(v1, v5, color);
    lines.emplace_back(v2, v6, color);
    lines.emplace_back(v3, v7, color);
}

void DebugRenderer::drawWireQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    const f32v2 topRight = origin + dims;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, origin + f32v2(dims.x, 0.0f), color);
    lines.emplace_back(origin, origin + f32v2(0.0f, dims.y), color);
    lines.emplace_back(topRight, topRight - f32v2(dims.x, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v2(0.0f, dims.x), color);

}

void DebugRenderer::drawWireQuad(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    ASSERT_RENDER_THREAD();
    const f32v3 topRight = origin + f32v3(dims.x, dims.y, 0.0f);
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, origin + f32v3(dims.x, 0.0f, 0.0f), color);
    lines.emplace_back(origin, origin + f32v3(0.0f, dims.y, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v3(dims.x, 0.0f, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v3(0.0f, dims.x, 0.0f), color);
}

void DebugRenderer::drawFilledQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    ASSERT_RENDER_THREAD();
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.emplace_back(origin, dims, color);
}

void DebugRenderer::drawFilledQuad(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.emplace_back(origin, dims, color);
}

void DebugRenderer::drawWireTriangle(const f32v3& v0, const f32v3& v1, const f32v3& v2, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(v0, v1, color);
    lines.emplace_back(v1, v2, color);
    lines.emplace_back(v2, v0, color);
}

void DebugRenderer::reserveFilledQuads(ui32 count, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.reserve(quads.size() + count);
}

void DebugRenderer::reserveLines(ui32 count, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.reserve(lines.size() + count);
}


void DebugRenderer::drawAABB(const i32AABB3& aabb, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    f32AABB3 aabbf;
    aabbf.pos = aabb.pos;
    aabbf.dims = aabb.dims;
    drawAABB(aabbf, color, lifeTime, id);
}

void DebugRenderer::drawAABB(const f32AABB3& aabb, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.reserve(lines.size() + 12);
    // Bottom 4
    const f32v3 v0(aabb.x, aabb.y, aabb.z);
    const f32v3 v1(aabb.x + aabb.dims.x, aabb.y, aabb.z);
    const f32v3 v2(aabb.x + aabb.dims.x, aabb.y + aabb.dims.x, aabb.z);
    const f32v3 v3(aabb.x, aabb.y + aabb.dims.x, aabb.z);
    // Top 4
    const f32v3 v4(aabb.x, aabb.y, aabb.z + aabb.dims.z);
    const f32v3 v5(aabb.x + aabb.dims.x, aabb.y, aabb.z + aabb.dims.z);
    const f32v3 v6(aabb.x + aabb.dims.x, aabb.y + aabb.dims.x, aabb.z + aabb.dims.z);
    const f32v3 v7(aabb.x, aabb.y + aabb.dims.x, aabb.z + aabb.dims.z);
    // Bottom
    lines.emplace_back(v0, v1, color);
    lines.emplace_back(v1, v2, color);
    lines.emplace_back(v2, v3, color);
    lines.emplace_back(v3, v0, color);
    // Top
    lines.emplace_back(v4, v5, color);
    lines.emplace_back(v5, v6, color);
    lines.emplace_back(v6, v7, color);
    lines.emplace_back(v7, v4, color);
    // Middle
    lines.emplace_back(v0, v4, color);
    lines.emplace_back(v1, v5, color);
    lines.emplace_back(v2, v6, color);
    lines.emplace_back(v3, v7, color);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, f32 height, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    const f32v3 botLeft3D(botLeft.x, botLeft.y, height);
    const f32v3 botRight3D(botRight.x, botRight.y, height);
    const f32v3 topLeft3D(topLeft.x, topLeft.y, height);
    const f32v3 topRight3D(topRight.x, topRight.y, height);
    lines.emplace_back(botLeft3D, topLeft3D, color);
    lines.emplace_back(topLeft3D, topRight3D, color);
    lines.emplace_back(topRight3D, botRight3D, color);
    lines.emplace_back(botRight3D, botLeft3D, color);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& dims, f32 height, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    const f32v3 topLeft = f32v3(botLeft.x, botLeft.y, 0.0f) + f32v3(0.0f, dims.y, height);
    const f32v3 topRight = f32v3(botLeft.x, botLeft.y, 0.0f) + f32v3(dims.x, dims.y, height);
    const f32v3 botRight = f32v3(botLeft.x, botLeft.y, 0.0f) + f32v3(dims.x, 0.0f, height);
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(botLeft, topLeft, color);
    lines.emplace_back(topLeft, topRight, color);
    lines.emplace_back(topRight, botRight, color);
    lines.emplace_back(botRight, botLeft, color);
}

void DebugRenderer::drawAABB(const i32AABB2& aabb, f32 height, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    f32v2 fPos(aabb.pos);
    f32v2 fDims(aabb.dims);
    const f32v2& bottomLeft = fPos;
    const f32v2& topRight = fPos + fDims;
    const f32v2 topLeft = fPos + f32v2(0.0f, fDims.y);
    const f32v2 bottomRight = fPos + f32v2(fDims.x, 0.0f);
    drawAABB(bottomLeft, bottomRight, topLeft, topRight, height, color, lifeTime);
}

void DebugRenderer::drawPath(const std::vector<f32v3>& path, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    if (path.size() < 2) {
        return;
    }
    OVERFLOW_ASSERT_UI32(path.size());

    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.reserve(lines.size() + path.size());

    for (ui32 i = 0; i < path.size() - 1; ++i) {
        const f32v3& pointA(path[i]);
        const f32v3& pointB(path[i + 1]);

        lines.emplace_back(
            pointA,
            pointB,
            color
        );
    }
}

void DebugRenderer::drawCircle(const f32v3& origin, f32 radius, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    ASSERT_RENDER_THREAD();
    auto&& circles = sNewCircles[std::make_pair(lifeTime, id)];
    circles.emplace_back(origin, radius, color);
}

void DebugRenderer::render(const f32v3& cameraPos, const f32m4& viewMatrix)
{
    ASSERT_RENDER_THREAD();

    vg::sBlendStates.ALPHA.set();

    // context in
    for (auto&& lineIt : sNewLines) {
        SimpleMesh newMesh;
        GL.glCreateVertexArrays(1, &newMesh.vao);
        GL.glCreateBuffers(1, &newMesh.vbo);
        auto&& lines = lineIt.second;

        newMesh.lifetime = lineIt.first.first;
        newMesh.id = lineIt.first.second;
        newMesh.numVerts = (GLsizei)(lines.size() * 2);
        newMesh.type = DebugMeshType::LINES;

        std::vector<SimpleMeshVertex> lineVertices(newMesh.numVerts);
        if (!lineVertices.size()) {
            continue;
        }

        int index = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            auto&& l = lines[i];
            lineVertices[index].position = l.position1;
            lineVertices[index].color = l.color;
            lineVertices[index + 1].position = l.position2;
            lineVertices[index + 1].color = l.color;
            index += 2;
        }
        GL.glNamedBufferStorage(newMesh.vbo, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data(), 0);
        GL.glVertexArrayVertexBuffer(newMesh.vao, 0, newMesh.vbo, 0, sizeof(SimpleMeshVertex));
        bindSimpleMeshVertexAttribs(newMesh.vao);

        sDebugMeshes.emplace_back(std::move(newMesh));
    }
    sNewLines.clear();

    // Thread safe
    {
        std::lock_guard<std::mutex> lockGuard(sNewLinesThreadSafeMutex);
        // context in
        for (auto&& lineIt : sNewLinesThreadSafe) {
            SimpleMesh newMesh;
            GL.glCreateVertexArrays(1, &newMesh.vao);
            GL.glCreateBuffers(1, &newMesh.vbo);
            auto&& lines = lineIt.second;

            newMesh.lifetime = lineIt.first.first;
            newMesh.id = lineIt.first.second;
            newMesh.numVerts = (GLsizei)(lines.size() * 2);
            newMesh.type = DebugMeshType::LINES;

            std::vector<SimpleMeshVertex> lineVertices(newMesh.numVerts);
            if (!lineVertices.size()) {
                continue;
            }

            int index = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                auto&& l = lines[i];
                lineVertices[index].position = l.position1;
                lineVertices[index].color = l.color;
                lineVertices[index + 1].position = l.position2;
                lineVertices[index + 1].color = l.color;
                index += 2;
            }
            GL.glNamedBufferStorage(newMesh.vbo, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data(), 0);
            GL.glVertexArrayVertexBuffer(newMesh.vao, 0, newMesh.vbo, 0, sizeof(SimpleMeshVertex));
            bindSimpleMeshVertexAttribs(newMesh.vao);

            sDebugMeshes.emplace_back(std::move(newMesh));
        }
        sNewLinesThreadSafe.clear();
    }

    for (auto&& quadIt : sNewQuads) {
        SimpleMesh newMesh;
        GL.glCreateVertexArrays(1, &newMesh.vao);
        GL.glCreateBuffers(1, &newMesh.vbo);
        auto&& quads = quadIt.second;

        newMesh.lifetime = quadIt.first.first;
        newMesh.id = quadIt.first.second;
        newMesh.numVerts = quads.size() * 4;
        newMesh.type = DebugMeshType::QUADS;

        std::vector<SimpleMeshVertex> quadVertices(quads.size() * 4);
        if (!quadVertices.size()) {
            continue;
        }

        int index = 0;
        for (size_t i = 0; i < quads.size(); ++i) {
            auto&& q = quads[i];
            // TODO: Time instead of frames
            quadVertices[index].position = q.position;
            quadVertices[index].color = q.color;
            quadVertices[index + 1].position = q.position + f32v3(q.dims.x, 0.0f, 0.0f);
            quadVertices[index + 1].color = q.color;
            quadVertices[index + 2].position = q.position + f32v3(q.dims.x, q.dims.y, 0.0f);
            quadVertices[index + 2].color = q.color;
            quadVertices[index + 3].position = q.position + f32v3(0.0f, q.dims.y, 0.0f);
            quadVertices[index + 3].color = q.color;
            index += 4;
        }
        GL.glNamedBufferStorage(newMesh.vbo, quadVertices.size() * sizeof(SimpleMeshVertex), quadVertices.data(), 0);
        GL.glVertexArrayVertexBuffer(newMesh.vao, 0, newMesh.vbo, 0, sizeof(SimpleMeshVertex));
        bindSimpleMeshVertexAttribs(newMesh.vao);
        sDebugMeshes.emplace_back(std::move(newMesh));
    }
    sNewQuads.clear();

    glDepthFunc((VGEnum)vg::DepthFunction::ALWAYS);

    // Quad meshes
    if (!sGlobalSimpleProgram.isCreated()) {
        initGlobalSimpleProgram();
    }
    sGlobalSimpleProgram.use();
    GL.glUniformMatrix4fv(sGlobalSimpleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
    GL.glUniform3fv(sGlobalSimpleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
    for (size_t i = 0; i < sDebugMeshes.size();) {
        auto&& mesh = sDebugMeshes[i];
        GL.glBindVertexArray(mesh.vao);
        if (mesh.type == DebugMeshType::LINES) {
            glLineWidth(1.0f);
            glVertexArrayElementBuffer(mesh.vao, 0);
            glDrawArrays(GL_LINES, 0, (GLsizei)mesh.numVerts);
            RenderStats::recordDrawCall(mesh.numVerts / 2);
        }
        // Quads
        else {
            glVertexArrayElementBuffer(mesh.vao, ProceduralMeshBuilder::sQuadIboUI32);
            glDrawElements(GL_TRIANGLES, (GLsizei)(mesh.numVerts * 6) / 4, GL_UNSIGNED_INT, 0);
            RenderStats::recordDrawCall(mesh.numVerts / 2);
        }

        if (mesh.lifetime <= 0) {
            GL.glDeleteBuffers(1, &mesh.vbo);
            GL.glDeleteVertexArrays(1, &mesh.vao);
            sDebugMeshes[i] = sDebugMeshes.back();
            sDebugMeshes.pop_back();
        }
        else {
            --mesh.lifetime;
            ++i;
        }
    }

    sGlobalSimpleProgram.unuse();

    for (auto&& circleIt : sNewCircles) {
        SimpleMesh newMesh;
        GL.glCreateVertexArrays(1, &newMesh.vao);
        GL.glCreateBuffers(1, &newMesh.vbo);
        auto&& circles = circleIt.second;

        newMesh.lifetime = circleIt.first.first;
        newMesh.id = circleIt.first.second;
        newMesh.numVerts = circles.size() * 4;
        newMesh.type = DebugMeshType::QUADS;

        std::vector<SimpleMeshCircleVertex> circleVertices(circles.size() * 4);

        int index = 0;
        for (size_t i = 0; i < circles.size(); ++i) {
            auto&& c = circles[i];
            // TODO: Time instead of frames
            circleVertices[index].position = c.position + f32v3(-c.radius, -c.radius, 0.0f);
            circleVertices[index].color = c.color;
            circleVertices[index].radius = c.radius;
            circleVertices[index].offset = f32v2(-c.radius, -c.radius);
            circleVertices[index + 1].position = c.position + f32v3(c.radius, -c.radius, 0.0f);
            circleVertices[index + 1].color = c.color;
            circleVertices[index + 1].radius = c.radius;
            circleVertices[index + 1].offset = f32v2(c.radius, -c.radius);
            circleVertices[index + 2].position = c.position + f32v3(c.radius, c.radius, 0.0f);
            circleVertices[index + 2].color = c.color;
            circleVertices[index + 2].radius = c.radius;
            circleVertices[index + 2].offset = f32v2(c.radius, c.radius);
            circleVertices[index + 3].position = c.position + f32v3(-c.radius, c.radius, 0.0f);
            circleVertices[index + 3].color = c.color;
            circleVertices[index + 3].radius = c.radius;
            circleVertices[index + 3].offset = f32v2(-c.radius, c.radius);
            index += 4;
        }
        GL.glNamedBufferStorage(newMesh.vbo, circleVertices.size() * sizeof(SimpleMeshCircleVertex), circleVertices.data(), 0);
        GL.glVertexArrayVertexBuffer(newMesh.vao, 0, newMesh.vbo, 0, sizeof(SimpleMeshCircleVertex));
        GL.glVertexArrayElementBuffer(newMesh.vao, ProceduralMeshBuilder::sQuadIboUI32);
        bindCircleMeshVertexAttribs(newMesh.vao);
        sDebugCircleMeshes.emplace_back(std::move(newMesh));
    }
    sNewCircles.clear();



    // Circle meshes
    if (!sGlobalCircleProgram.isCreated()) {
        initGlobalCircleProgram();
    }
    sGlobalCircleProgram.use();
    GL.glUniformMatrix4fv(sGlobalCircleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
    GL.glUniform3fv(sGlobalCircleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
    for (size_t i = 0; i < sDebugCircleMeshes.size();) {
        auto&& mesh = sDebugCircleMeshes[i];

        GL.glBindVertexArray(mesh.vao);
       
        glDrawElements(GL_TRIANGLES, (GLsizei)(mesh.numVerts * 6) / 4, GL_UNSIGNED_INT, nullptr);
        RenderStats::recordDrawCall(mesh.numVerts / 2);

        if (mesh.lifetime <= 0) {
            GL.glDeleteBuffers(1, &mesh.vbo);
            GL.glDeleteVertexArrays(1, &mesh.vao);
            mesh.vbo = 0;
            sDebugCircleMeshes[i] = sDebugCircleMeshes.back();
            sDebugCircleMeshes.pop_back();
        }
        else {
            --mesh.lifetime;
            ++i;
        }
    }

    vg::BlendState::restorePrevious();
}

void DebugRenderer::clearAllMeshesWithId(int id)
{
    for (size_t i = 0; i < sDebugMeshes.size();) {
        auto&& mesh = sDebugMeshes[i];
        if (mesh.id == id) {
            GL.glDeleteBuffers(1, &mesh.vbo);
            GL.glDeleteVertexArrays(1, &mesh.vao);
            sDebugMeshes[i] = sDebugMeshes.back();
            sDebugMeshes.pop_back();
        }
        else {
            ++i;
        }
    }
    for (size_t i = 0; i < sDebugCircleMeshes.size();) {
        auto&& mesh = sDebugCircleMeshes[i];
        if (mesh.id == id) {
            GL.glDeleteBuffers(1, &mesh.vbo);
            GL.glDeleteVertexArrays(1, &mesh.vao);
            sDebugCircleMeshes[i] = sDebugCircleMeshes.back();
            sDebugCircleMeshes.pop_back();
        }
        else {
            ++i;
        }
    }
}

void DebugRenderer::clearAll()
{
    for (auto&& mesh : sDebugMeshes) {
        GL.glDeleteBuffers(1, &mesh.vbo);
        GL.glDeleteVertexArrays(1, &mesh.vao);
    }
    for (auto&& mesh : sDebugCircleMeshes) {
        GL.glDeleteBuffers(1, &mesh.vbo);
        GL.glDeleteVertexArrays(1, &mesh.vao);
    }
    sDebugMeshes.clear();
    sDebugCircleMeshes.clear();
}
