#include "stdafx.h"
#include "DebugRenderer.h"

#include <Vorb/MeshGenerators.h>
#include <Vorb/graphics/RasterizerState.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/ShaderManager.h>
#include <glm/gtx/rotate_vector.hpp>

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
    f32v2 position1;
    f32v2 position2;
    color4 color;
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
void DebugRenderer::drawVector(const f32v2& origin, const f32v2& vec, color4 color)
{
    const f32v2 end = origin + vec;
    const f32v2 tipRay = -vec * 0.2f;
    sLines.push_back({origin, end, color });
    sLines.push_back({ end, end + glm::rotate(tipRay, rotVal), color });
    sLines.push_back({ end, end + glm::rotate(tipRay, -rotVal), color });
}

void DebugRenderer::drawLine(const f32v2& origin, const f32v2& vec, color4 color)
{
	const f32v2 end = origin + vec;
	const f32v2 tipRay = -vec * 0.2f;
	sLines.push_back({ origin, end, color });
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
    std::vector<SimpleMeshVertex> m_vertices(sLines.size() * 2);

    int index = 0;
    for (auto& l : sLines) {
        m_vertices[index].position = l.position1;
        m_vertices[index].color = l.color;
        m_vertices[index + 1].position = l.position2;
        m_vertices[index + 1].color = l.color;
        index += 2;
    }

    glBindBuffer(GL_ARRAY_BUFFER, sVbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertices.size() * sizeof(SimpleMeshVertex), m_vertices.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glLineWidth(2.0f);

    glVertexAttribPointer(sProgram.getAttribute("vPosition"), 2, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
    glVertexAttribPointer(sProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
    glUniformMatrix4fv(sProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);

    glDrawArrays(GL_LINES, 0, m_vertices.size());

    sLines.clear();

    // out
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sProgram.disableVertexAttribArrays();
    sProgram.use();
}
