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

struct Line {
    Line(const f32v2& pos1, const f32v2& pos2, const color4& colr, int lifetime)
        : position1(pos1)
        , position2(pos2)
        , color(colr)
        , lifeTime(lifetime) {
    }
    f32v2 position1;
    f32v2 position2;
    color4 color;
    int lifeTime;
};

struct DebugQuad {
    DebugQuad(const f32v2& position, const f32v2& dims, const color4& colr, int lifetime)
        : position(position)
        , dims(dims)
        , color(colr)
        , lifeTime(lifetime) {
    }
    f32v2 position;
    f32v2 dims;
    color4 color;
    int lifeTime;
};

struct SimpleMeshVertex {
    f32v2 position;
    color4 color;
};

std::vector<Line> sLines;
std::vector<DebugQuad> sQuads;
vg::GLProgram sProgram;
VGBuffer sVboLines = 0;
VGBuffer sVboQuads = 0;

const float rotVal = glm::radians(30.0f);
void DebugRenderer::drawVector(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/)
{
    const f32v2 end = origin + vec;
    const f32v2 tipRay = -vec * 0.2f;
    sLines.emplace_back(origin, end, color, lifeTime );
    sLines.emplace_back(end, end + glm::rotate(tipRay, rotVal), color, lifeTime);
    sLines.emplace_back(end, end + glm::rotate(tipRay, -rotVal), color, lifeTime);
}

void DebugRenderer::drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime/* = 0*/)
{
	const f32v2 end = origin + vec;
	const f32v2 tipRay = -vec * 0.2f;
	sLines.emplace_back(origin, end, color, lifeTime);
}

void DebugRenderer::drawBox(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/) {

    const f32v2 topRight = origin + dims;
    sLines.emplace_back(origin, origin + f32v2(dims.x, 0.0f), color, lifeTime);
    sLines.emplace_back(origin, origin + f32v2(0.0f, dims.y), color, lifeTime);
    sLines.emplace_back(topRight, topRight - f32v2(dims.x, 0.0f), color, lifeTime);
    sLines.emplace_back(topRight, topRight - f32v2(0.0f, dims.x), color, lifeTime);

}

void DebugRenderer::drawQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime /*= 0*/)
{
    sQuads.emplace_back(origin, dims, color, lifeTime);
}

void DebugRenderer::drawAABB(const b2AABB& aabb, color4 color, int lifeTime /*= 0*/) {

	const f32v2& bottomLeft = TO_VVEC2_C(aabb.lowerBound);
	const f32v2& topRight = TO_VVEC2_C(aabb.upperBound);
	const f32v2 topLeft = f32v2(bottomLeft.x, topRight.y);
	const f32v2 bottomRight = f32v2(topRight.x, bottomLeft.y);

    drawAABB(bottomLeft, bottomRight, topLeft, topRight, color, lifeTime);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, color4 color, int lifeTime /*= 0*/) {
	sLines.emplace_back(botLeft, topLeft, color, lifeTime);
	sLines.emplace_back(topLeft, topRight, color, lifeTime);
	sLines.emplace_back(topRight, botRight, color, lifeTime);
	sLines.emplace_back(botRight, botLeft, color, lifeTime);
}

void DebugRenderer::drawAABB(const f32v2& botLeft, const f32v2& dims, color4 color, int lifeTime /*= 0*/)
{
    const f32v2 topLeft = botLeft + f32v2(0.0f, dims.y);
    const f32v2 topRight = botLeft + f32v2(dims.x, dims.y);
    const f32v2 botRight = botLeft + f32v2(dims.x, 0.0f);
    sLines.emplace_back(botLeft, topLeft, color, lifeTime);
    sLines.emplace_back(topLeft, topRight, color, lifeTime);
    sLines.emplace_back(topRight, botRight, color, lifeTime);
    sLines.emplace_back(botRight, botLeft, color, lifeTime);
}

void DebugRenderer::renderLines(const f32m4& viewMatrix)
{
    if (!sLines.size() || !sQuads.size()) {
        return;
    }

    if (!sProgram.isCreated()) {
        sProgram = vg::ShaderManager::createProgram(VERT_SRC, FRAG_SRC);
        glGenBuffers(1, &sVboLines);
        glGenBuffers(1, &sVboQuads);
    }

    sProgram.use();
    sProgram.enableVertexAttribArrays();

    // context in
    std::vector<SimpleMeshVertex> lineVertices(sLines.size() * 2);
    std::vector<SimpleMeshVertex> quadVertices(sQuads.size() * 4);
    std::vector<Line> persistantLines;
    std::vector<DebugQuad> persistantQuads;

    int index = 0;
    for (auto& q : sQuads) {
        // TODO: Time instead of frames
        if (--q.lifeTime > 0) {
            persistantQuads.emplace_back(q);
        }
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

    index = 0;
    for (auto& l : sLines) {
        // TODO: Time instead of frames
        if (--l.lifeTime > 0) {
            persistantLines.emplace_back(l);
        }
        lineVertices[index].position = l.position1;
        lineVertices[index].color = l.color;
        lineVertices[index + 1].position = l.position2;
        lineVertices[index + 1].color = l.color;
        index += 2;
    }

    glDepthFunc((VGEnum)vg::DepthFunction::ALWAYS);
    // Lines
    {
        glBindBuffer(GL_ARRAY_BUFFER, sVboLines);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glLineWidth(2.0f);

        glVertexAttribPointer(sProgram.getAttribute("vPosition"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
        glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
        glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);

        glDrawArrays(GL_LINES, 0, (GLsizei)lineVertices.size());

        sLines.swap(persistantLines);
    }

    // Quads
    {
        glBindBuffer(GL_ARRAY_BUFFER, sVboQuads);
        glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, quadVertices.size() * sizeof(SimpleMeshVertex), quadVertices.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glVertexAttribPointer(sProgram.getAttribute("vPosition"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
        glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
        glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);

        glDrawArrays(GL_QUADS, 0, (GLsizei)quadVertices.size());

        sQuads.swap(persistantQuads);
    }

    // out
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sProgram.disableVertexAttribArrays();
    sProgram.use();
}
