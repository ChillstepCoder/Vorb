#include "stdafx.h"
#include "debugging/DebugMesh.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/ShaderManager.h>

namespace {
    static const cString SIMPLE_VERT_SRC = R"(
// Uniforms
uniform mat4 unWVP;
uniform vec3 CameraPos;
// Input
layout(location = 0) in vec4 vPosition; // Position in object space
layout(location = 1) in vec4 vColor;
out vec4 fColor;
void main() {
  fColor = vColor;
  gl_Position = unWVP * (vPosition - vec4(CameraPos, 0.0));
}
)";
    static const cString SIMPLE_FRAG_SRC = R"(
in vec4 fColor;
// Output
out vec4 pColor;
void main() {
  pColor = fColor;
}
)";

    static const cString VERT_CIRCLE_SRC = R"(
// Uniforms
uniform mat4 unWVP;
uniform vec3 CameraPos;
// Input
layout(location = 0) in vec4 vPosition; // Position in object space
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vRadius;
layout(location = 3) in vec2 vOffset;
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
    static const cString FRAG_CIRCLE_SRC = R"(
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

vg::GLProgram sGlobalSimpleProgram;
vg::GLProgram sGlobalCircleProgram;

void initGlobalSimpleProgram() {
    sGlobalSimpleProgram = vg::ShaderManager::createProgram(SIMPLE_VERT_SRC, SIMPLE_FRAG_SRC, nullptr);
}

void initGlobalCircleProgram() {
    sGlobalCircleProgram = vg::ShaderManager::createProgram(VERT_CIRCLE_SRC, FRAG_CIRCLE_SRC, nullptr);
}
