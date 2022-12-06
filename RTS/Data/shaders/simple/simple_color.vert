#include "../GlobalUbo.glsl"

// Input
in vec4 vPosition; // Position in screen space
in vec4 vTint;

out vec4 fTint;

void main() {
  fTint = vTint;
  gl_Position = VP * (vPosition - vec4(CameraPos, 0.0));
}