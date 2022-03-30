#include "stdafx.h"
#include "DebugRenderer.h"

#include <Vorb/MeshGenerators.h>
#include <Vorb/graphics/RasterizerState.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/SpriteFont.h>
#include <glm/gtx/rotate_vector.hpp>
#include <box2d/b2_collision.h>
#include "pathfinding/NavPath.h"

#include "world/WorldGrid.h" // For terrain height data

#include "rendering/RenderStats.h"

namespace {
    const cString VERT_SRC = R"(
// Uniforms
uniform mat4 unWVP;
uniform vec3 CameraPos;
// Input
in vec4 vPosition; // Position in object space
in vec4 vColor;
out vec4 fColor;
void main() {
  fColor = vColor;
  gl_Position = unWVP * (vPosition - vec4(CameraPos, 0.0));
}
)";
    const cString FRAG_SRC = R"(
in vec4 fColor;
// Output
out vec4 pColor;
void main() {
  pColor = fColor;
}
)";

    const cString VERT_CIRCLE_SRC = R"(
// Uniforms
uniform mat4 unWVP;
uniform vec3 CameraPos;
// Input
in vec4 vPosition; // Position in object space
in vec4 vColor;
in float vRadius;
in vec2 vOffset;
out vec4 fColor;
out float fRadius;
out vec2 fOffset;
void main() {
  fColor = vColor;
  fRadius = vRadius;
  fOffset = vOffset;
  gl_Position = unWVP * (vPosition - vec4(CameraPos, 0.0));
}
)";
    const cString FRAG_CIRCLE_SRC = R"(
in vec4 fColor;
in float fRadius;
in vec2 fOffset;
// Output
out vec4 pColor;
void main() {
  vec2 scaled = fOffset / fRadius;
  float slength = length(scaled);
  pColor = fColor;
  if (slength > 1.00 || slength < 0.93) {
    discard;
  }
}
)";
}

struct DebugLine {
    DebugLine(const f32v2& pos1, const f32v2& pos2, const color4& colr)
        : position1(pos1.x, pos1.y, 0.0f)
        , position2(pos2.x, pos2.y, 0.0f)
        , color(colr) {
    }
    DebugLine(const f32v3& pos1, const f32v3& pos2, const color4& colr)
        : position1(pos1)
        , position2(pos2)
        , color(colr) {
    }
    f32v3 position1;
    f32v3 position2;
    color4 color;
};

struct DebugQuad {
    DebugQuad(const f32v2& position, const f32v2& dims, const color4& colr)
        : position(position.x, position.y, 0.0f)
        , dims(dims)
        , color(colr) {
    }
    DebugQuad(const f32v3& position, const f32v2& dims, const color4& colr)
        : position(position)
        , dims(dims)
        , color(colr) {
    }
    f32v3 position;
    f32v2 dims;
    color4 color;
};

struct DebugCircle {
    DebugCircle(const f32v3& position, const f32 radius, const color4& colr)
        : position(position)
        , radius(radius)
        , color(colr) {
    }
    f32v3 position;
    f32 radius;
    color4 color;
};

struct SimpleMeshVertex {
    f32v3 position;
    color4 color;
};

struct SimpleMeshCircleVertex {
    f32v3 position;
    f32v2 offset;
    f32 radius;
    color4 color;
};

enum class DebugMeshType {
    LINES,
    QUADS
};
struct SimpleMesh {
    VGBuffer vbo;
    DebugMeshType type;
    int lifetime;
    int id;
    GLsizei numVerts;
};

vg::GLProgram sProgram;
vg::GLProgram sCircleProgram;

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
    assert(IS_MAIN_THREAD());
    const f32v2 end = origin + vec;
    const f32v2 tipRay = -vec * 0.2f;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
    lines.emplace_back(end, end + glm::rotate(tipRay, rotVal), color);
    lines.emplace_back(end, end + glm::rotate(tipRay, -rotVal), color);
}

void DebugRenderer::drawVector(const f32v3& origin, const f32v3& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    assert(IS_MAIN_THREAD());
    const f32v3 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    assert(IS_MAIN_THREAD());
	const f32v2 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLine(const f32v3& origin, const f32v3& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    assert(IS_MAIN_THREAD());
    const f32v3 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPoints(const f32v2& origin, const f32v2& end, color4 color, int lifeTime/* = 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPoints(const f32v3& origin, const f32v3& end, const color4& color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPointsThreadSafe(const f32v3& origin, const f32v3& end, const color4& color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(!IS_MAIN_THREAD());
    std::lock_guard<std::mutex> lockGuard(sNewLinesThreadSafeMutex);
    auto&& lines = sNewLinesThreadSafe[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawWireQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    const f32v2 topRight = origin + dims;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, origin + f32v2(dims.x, 0.0f), color);
    lines.emplace_back(origin, origin + f32v2(0.0f, dims.y), color);
    lines.emplace_back(topRight, topRight - f32v2(dims.x, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v2(0.0f, dims.x), color);

}

void DebugRenderer::drawWireQuad(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    assert(IS_MAIN_THREAD());
    const f32v3 topRight = origin + f32v3(dims.x, dims.y, 0.0f);
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, origin + f32v3(dims.x, 0.0f, 0.0f), color);
    lines.emplace_back(origin, origin + f32v3(0.0f, dims.y, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v3(dims.x, 0.0f, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v3(0.0f, dims.x, 0.0f), color);
}

void DebugRenderer::drawFilledQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    assert(IS_MAIN_THREAD());
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.emplace_back(origin, dims, color);
}

void DebugRenderer::drawFilledQuad(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.emplace_back(origin, dims, color);
}

void DebugRenderer::drawWireTriangle(const f32v3& v0, const f32v3& v1, const f32v3& v2, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(v0, v1, color);
    lines.emplace_back(v1, v2, color);
    lines.emplace_back(v2, v0, color);
}

void DebugRenderer::reserveFilledQuads(ui32 count, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.reserve(quads.size() + count);
}

void DebugRenderer::reserveLines(ui32 count, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.reserve(lines.size() + count);
}

void DebugRenderer::drawAABB(const b2AABB& aabb, f32 height, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
	const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
	const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
	const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
	const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

    drawAABB(bottomLeft, bottomRight, topLeft, topRight, height, color, lifeTime);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, f32 height, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
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
    assert(IS_MAIN_THREAD());
    const f32v3 topLeft = f32v3(botLeft.x, botLeft.y, 0.0f) + f32v3(0.0f, dims.y, height);
    const f32v3 topRight = f32v3(botLeft.x, botLeft.y, 0.0f) + f32v3(dims.x, dims.y, height);
    const f32v3 botRight = f32v3(botLeft.x, botLeft.y, 0.0f) + f32v3(dims.x, 0.0f, height);
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(botLeft, topLeft, color);
    lines.emplace_back(topLeft, topRight, color);
    lines.emplace_back(topRight, botRight, color);
    lines.emplace_back(botRight, botLeft, color);
}

void DebugRenderer::drawAABB(const ui32AABB2& aabb, f32 height, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    f32v2 fPos(aabb.pos);
    f32v2 fDims(aabb.dims);
    const f32v2& bottomLeft = fPos;
    const f32v2& topRight = fPos + fDims;
    const f32v2 topLeft = fPos + f32v2(0.0f, fDims.y);
    const f32v2 bottomRight = fPos + f32v2(fDims.x, 0.0f);
    drawAABB(bottomLeft, bottomRight, topLeft, topRight, height, color, lifeTime);
}

void DebugRenderer::drawPath(const NavPath& path, color4 color, const WorldGrid& worldGrid, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    ui32 numPoints = path.getNumPoints();
    const PathPoint* points = path.getPoints();
    if (numPoints < 2) {
        return;
    }
    OVERFLOW_ASSERT_UI32(numPoints);

    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.reserve(lines.size() + numPoints);

    for (ui32 i = 0; i < numPoints - 1; ++i) {
        const f32v2 pointA(points[i].x + 0.5f, points[i].y + 0.5f);
        const f32v2 pointB(points[i + 1].x + 0.5f, points[i + 1].y + 0.5f);

        lines.emplace_back(
            f32v3(pointA.x, pointA.y, worldGrid.tryComputeHeightAtPoint(pointA)),
            f32v3(pointB.x, pointB.y, worldGrid.tryComputeHeightAtPoint(pointB)),
            color
        );
    }
}

void DebugRenderer::drawCircle(const f32v3& origin, f32 radius, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    assert(IS_MAIN_THREAD());
    auto&& circles = sNewCircles[std::make_pair(lifeTime, id)];
    circles.emplace_back(origin, radius, color);
}

void DebugRenderer::render(const f32v3& cameraPos, const f32m4& viewMatrix)
{
    assert(IS_MAIN_THREAD());

    // Quad meshes
    if (!sProgram.isCreated()) {
        sProgram = vg::ShaderManager::createProgram(VERT_SRC, FRAG_SRC);
    }

    sProgram.use();
    sProgram.enableVertexAttribArrays();

    // context in
    for (auto&& lineIt : sNewLines) {
        SimpleMesh newMesh;
        auto&& lines = lineIt.second;

        newMesh.lifetime = lineIt.first.first;
        newMesh.id = lineIt.first.second;
        glGenBuffers(1, &newMesh.vbo);
        newMesh.numVerts = (GLsizei)(lines.size() * 2);
        newMesh.type = DebugMeshType::LINES;

        std::vector<SimpleMeshVertex> lineVertices(newMesh.numVerts);

        int index = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            auto&& l = lines[i];
            lineVertices[index].position = l.position1;
            lineVertices[index].color = l.color;
            lineVertices[index + 1].position = l.position2;
            lineVertices[index + 1].color = l.color;
            index += 2;
        }
        glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data());

        sDebugMeshes.emplace_back(std::move(newMesh));
    }
    sNewLines.clear();

    // Thread safe
    {
        std::lock_guard<std::mutex> lockGuard(sNewLinesThreadSafeMutex);
        // context in
        for (auto&& lineIt : sNewLinesThreadSafe) {
            SimpleMesh newMesh;
            auto&& lines = lineIt.second;

            newMesh.lifetime = lineIt.first.first;
            newMesh.id = lineIt.first.second;
            glGenBuffers(1, &newMesh.vbo);
            newMesh.numVerts = (GLsizei)(lines.size() * 2);
            newMesh.type = DebugMeshType::LINES;

            std::vector<SimpleMeshVertex> lineVertices(newMesh.numVerts);

            int index = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                auto&& l = lines[i];
                lineVertices[index].position = l.position1;
                lineVertices[index].color = l.color;
                lineVertices[index + 1].position = l.position2;
                lineVertices[index + 1].color = l.color;
                index += 2;
            }
            glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data());

            sDebugMeshes.emplace_back(std::move(newMesh));
        }
        sNewLinesThreadSafe.clear();
    }

    for (auto&& quadIt : sNewQuads) {
        SimpleMesh newMesh;
        auto&& quads = quadIt.second;

        newMesh.lifetime = quadIt.first.first;
        newMesh.id = quadIt.first.second;
        glGenBuffers(1, &newMesh.vbo);
        newMesh.numVerts = quads.size() * 4;
        newMesh.type = DebugMeshType::QUADS;

        std::vector<SimpleMeshVertex> quadVertices(quads.size() * 4);

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
        glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, quadVertices.size() * sizeof(SimpleMeshVertex), quadVertices.data());
        sDebugMeshes.emplace_back(std::move(newMesh));
    }
    sNewQuads.clear();

    std::set<VGBuffer> buffer;
    for (auto&& i : sDebugMeshes) {
        assert(buffer.find(i.vbo) == buffer.end());
        buffer.insert(i.vbo);
    }

    glDepthFunc((VGEnum)vg::DepthFunction::ALWAYS);
    
    for (size_t i = 0; i < sDebugMeshes.size();) {
        auto&& mesh = sDebugMeshes[i];
        // Lines
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glVertexAttribPointer(sProgram.getAttribute("vPosition"), 3, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
        glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
        glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
        glUniform3fv(sProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
        if (mesh.type == DebugMeshType::LINES) {
            glLineWidth(1.0f);
            glDrawArrays(GL_LINES, 0, (GLsizei)mesh.numVerts);
            RenderStats::recordDrawCall(mesh.numVerts / 2);
        }
        // Quads
        else {
            glDrawArrays(GL_QUADS, 0, (GLsizei)mesh.numVerts);
            RenderStats::recordDrawCall(mesh.numVerts / 2);
        }

        if (mesh.lifetime <= 0) {
            glDeleteBuffers(1, &mesh.vbo);
            sDebugMeshes[i] = sDebugMeshes.back();
            sDebugMeshes.pop_back();
        }
        else {
            --mesh.lifetime;
            ++i;
        }
    }

    sProgram.disableVertexAttribArrays();
    sProgram.unuse();

    // Circle meshes
    if (!sCircleProgram.isCreated()) {
        sCircleProgram = vg::ShaderManager::createProgram(VERT_CIRCLE_SRC, FRAG_CIRCLE_SRC);
    }

    sCircleProgram.use();
    sCircleProgram.enableVertexAttribArrays();

    for (auto&& circleIt : sNewCircles) {
        SimpleMesh newMesh;
        auto&& circles = circleIt.second;

        newMesh.lifetime = circleIt.first.first;
        newMesh.id = circleIt.first.second;
        glGenBuffers(1, &newMesh.vbo);
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
        glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(SimpleMeshCircleVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, circleVertices.size() * sizeof(SimpleMeshCircleVertex), circleVertices.data());
        sDebugCircleMeshes.emplace_back(std::move(newMesh));
    }
    sNewCircles.clear();

    for (size_t i = 0; i < sDebugCircleMeshes.size();) {
        auto&& mesh = sDebugCircleMeshes[i];
        // Lines
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glVertexAttribPointer(sCircleProgram.getAttribute("vPosition"), 3, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshCircleVertex), offsetptr(SimpleMeshCircleVertex, position));
        glVertexAttribPointer(sCircleProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshCircleVertex), offsetptr(SimpleMeshCircleVertex, color));
        glVertexAttribPointer(sCircleProgram.getAttribute("vRadius"), 1, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshCircleVertex), offsetptr(SimpleMeshCircleVertex, radius));
        glVertexAttribPointer(sCircleProgram.getAttribute("vOffset"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshCircleVertex), offsetptr(SimpleMeshCircleVertex, offset));
        glUniformMatrix4fv(sCircleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
        glUniform3fv(sCircleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
        glDrawArrays(GL_QUADS, 0, (GLsizei)mesh.numVerts);
        RenderStats::recordDrawCall(mesh.numVerts / 2);

        if (mesh.lifetime <= 0) {
            glDeleteBuffers(1, &mesh.vbo);
            mesh.vbo = 0;
            sDebugCircleMeshes[i] = sDebugCircleMeshes.back();
            sDebugCircleMeshes.pop_back();
        }
        else {
            --mesh.lifetime;
            ++i;
        }
    }

    // out
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sCircleProgram.disableVertexAttribArrays();
    sCircleProgram.unuse();
}

void DebugRenderer::clearAllMeshesWithId(int id)
{
    for (size_t i = 0; i < sDebugMeshes.size();) {
        auto&& mesh = sDebugMeshes[i];
        if (mesh.id == id) {
            glDeleteBuffers(1, &mesh.vbo);
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
            glDeleteBuffers(1, &mesh.vbo);
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
        glDeleteBuffers(1, &mesh.vbo);
    }
    for (auto&& mesh : sDebugCircleMeshes) {
        glDeleteBuffers(1, &mesh.vbo);
    }
    sDebugMeshes.clear();
    sDebugCircleMeshes.clear();
}
