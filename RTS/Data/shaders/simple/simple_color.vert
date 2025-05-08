#include "../GlobalUbo.glsl"

// Input
layout(location = 0) in vec4 vPosition; // Position in screen space
layout(location = 1) in vec4 vTint;

out vec4 fTint;
uniform mat4 unVP;
uniform vec3 unCameraPos = vec3(0.0, 0.0, 0.0);

void main() {
  fTint = vTint;
  gl_Position = unVP * (vPosition - vec4(unCameraPos, 0.0));
}