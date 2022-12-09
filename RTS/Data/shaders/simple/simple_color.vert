#include "../GlobalUbo.glsl"

// Input
layout(location = 0) in vec4 vPosition; // Position in screen space
layout(location = 1) in vec4 vTint;

out vec4 fTint;

void main() {
  fTint = vTint;
  gl_Position = VP * (vPosition - vec4(CameraPos, 0.0));
}