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
#include "pathfinding/PathFinder.h"

namespace {
    const cString VERT_SRC = R"(
// Uniforms
uniform mat4 unWVP;
// Input
in vec4 vPosition; // Position in object space
in vec4 vColor;
out vec4 fColor;
void main() {
  fColor = vColor;
  gl_Position = unWVP * vPosition;
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
}

struct DebugLine {
    DebugLine(const f32v2& pos1, const f32v2& pos2, const color4& colr)
        : position1(pos1)
        , position2(pos2)
        , color(colr) {
    }
    f32v2 position1;
    f32v2 position2;
    color4 color;
};

struct DebugQuad {
    DebugQuad(const f32v2& position, const f32v2& dims, const color4& colr)
        : position(position)
        , dims(dims)
        , color(colr) {
    }
    f32v2 position;
    f32v2 dims;
    color4 color;
};

struct SimpleMeshVertex {
    f32v2 position;
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

std::vector<SimpleMesh> sDebugMeshes;
std::map<std::pair<i32, i32> /*lifetime,id*/, std::vector<DebugLine>> sNewLines;
std::map<std::pair<i32, i32> /*lifetime,id*/, std::vector<DebugQuad>> sNewQuads;


const float rotVal = glm::radians(30.0f);
void DebugRenderer::drawVector(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    const f32v2 end = origin + vec;
    const f32v2 tipRay = -vec * 0.2f;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
    lines.emplace_back(end, end + glm::rotate(tipRay, rotVal), color);
    lines.emplace_back(end, end + glm::rotate(tipRay, -rotVal), color);
}

void DebugRenderer::drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
	const f32v2 end = origin + vec;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawLineBetweenPoints(const f32v2& origin, const f32v2& end, color4 color, int lifeTime/* = 0*/, int id /*= 0*/)
{
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, end, color);
}

void DebugRenderer::drawBox(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {

    const f32v2 topRight = origin + dims;
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(origin, origin + f32v2(dims.x, 0.0f), color);
    lines.emplace_back(origin, origin + f32v2(0.0f, dims.y), color);
    lines.emplace_back(topRight, topRight - f32v2(dims.x, 0.0f), color);
    lines.emplace_back(topRight, topRight - f32v2(0.0f, dims.x), color);

}

void DebugRenderer::drawQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    auto&& quads = sNewQuads[std::make_pair(lifeTime, id)];
    quads.emplace_back(origin, dims, color);
}

void DebugRenderer::drawAABB(const b2AABB& aabb, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {

	const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
	const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
	const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
	const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

    drawAABB(bottomLeft, bottomRight, topLeft, topRight, color, lifeTime);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {

    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(botLeft, topLeft, color);
    lines.emplace_back(topLeft, topRight, color);
    lines.emplace_back(topRight, botRight, color);
    lines.emplace_back(botRight, botLeft, color);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& dims, color4 color, int lifeTime /*= 0*/, int id /*= 0*/)
{
    const f32v2 topLeft = botLeft + f32v2(0.0f, dims.y);
    const f32v2 topRight = botLeft + f32v2(dims.x, dims.y);
    const f32v2 botRight = botLeft + f32v2(dims.x, 0.0f);
    auto&& lines = sNewLines[std::make_pair(lifeTime, id)];
    lines.emplace_back(botLeft, topLeft, color);
    lines.emplace_back(topLeft, topRight, color);
    lines.emplace_back(topRight, botRight, color);
    lines.emplace_back(botRight, botLeft, color);
}

void DebugRenderer::drawPath(const Path& path, color4 color, int lifeTime /*= 0*/, int id /*= 0*/) {
    if (path.numPoints < 2) {
        return;
    }
    for (ui32 i = 0; i < path.numPoints - 1; ++i) {
        drawLineBetweenPoints(
            f32v2(path.points[i])     + f32v2(0.5f, 0.5f),
            f32v2(path.points[i + 1]) + f32v2(0.5f, 0.5f),
            color,
            lifeTime,
            id
        );
    }
}

void DebugRenderer::render(const f32m4& viewMatrix)
{

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
        newMesh.numVerts = lines.size() * 2;
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
            quadVertices[index + 1].position = q.position + f32v2(q.dims.x, 0.0f);
            quadVertices[index + 1].color = q.color;
            quadVertices[index + 2].position = q.position + f32v2(q.dims.x, q.dims.y);
            quadVertices[index + 2].color = q.color;
            quadVertices[index + 3].position = q.position + f32v2(0.0f, q.dims.y);
            quadVertices[index + 3].color = q.color;
            index += 4;
        }
        glBindBuffer(GL_ARRAY_BUFFER, newMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, quadVertices.size() * sizeof(SimpleMeshVertex), quadVertices.data());
        sDebugMeshes.emplace_back(std::move(newMesh));
    }
    sNewQuads.clear();

    glDepthFunc((VGEnum)vg::DepthFunction::ALWAYS);
    
    for (size_t i = 0; i < sDebugMeshes.size();) {
        auto&& mesh = sDebugMeshes[i];
        // Lines
        if (mesh.type == DebugMeshType::LINES) {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glLineWidth(2.0f);

            glVertexAttribPointer(sProgram.getAttribute("vPosition"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
            glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
            glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);

            glDrawArrays(GL_LINES, 0, (GLsizei)mesh.numVerts);
        }
        // Quads
        else {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glVertexAttribPointer(sProgram.getAttribute("vPosition"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
            glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
            glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);

            glDrawArrays(GL_QUADS, 0, (GLsizei)mesh.numVerts);
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

    // out
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sProgram.disableVertexAttribArrays();
    sProgram.use();
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
    sDebugMeshes.clear();
}

void DebugRenderer::clearAll()
{
    for (auto&& mesh : sDebugMeshes) {
        glDeleteBuffers(1, &mesh.vbo);
    }
    sDebugMeshes.clear();
}
