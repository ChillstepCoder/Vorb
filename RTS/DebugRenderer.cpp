#include "stdafx.h"
#include "DebugRenderer.h"

#include <Vorb/MeshGenerators.h>
#include <Vorb/graphics/RasterizerState.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/ShaderManager.h>
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

struct SimpleMeshVertex {
    f32v2 position;
    color4 color;
};

std::vector<Line> sLines;
vg::GLProgram sProgram;
VGBuffer sVbo = 0;
VGIndexBuffer sIbo = 0;

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

void DebugRenderer::renderLines(const f32m4& viewMatrix)
{
    if (!sLines.size()) {
        return;
    }

    if (!sProgram.isCreated()) {
        sProgram = vg::ShaderManager::createProgram(VERT_SRC, FRAG_SRC);
        glGenBuffers(1, &sVbo);
        glGenBuffers(1, &sIbo);
    }

    sProgram.use();
    sProgram.enableVertexAttribArrays();

    // context in
    std::vector<SimpleMeshVertex> vertices(sLines.size() * 2);
    std::vector<Line> persistantLines;

    int index = 0;
    for (auto& l : sLines) {
        // TODO: Time instead of frames
        if (--l.lifeTime > 0) {
            persistantLines.emplace_back(l);
        }
        vertices[index].position = l.position1;
        vertices[index].color = l.color;
        vertices[index + 1].position = l.position2;
        vertices[index + 1].color = l.color;
        index += 2;
    }

    glBindBuffer(GL_ARRAY_BUFFER, sVbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(SimpleMeshVertex), vertices.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glLineWidth(2.0f);

    glVertexAttribPointer(sProgram.getAttribute("vPosition"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
    glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
    glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);

    glDrawArrays(GL_LINES, 0, (GLsizei)vertices.size());

    sLines.swap(persistantLines);

    // out
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sProgram.disableVertexAttribArrays();
    sProgram.use();
}
