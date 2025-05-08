// Input
layout(location = 0) in vec4 vPosition; // Position in world space
layout(location = 7) in mat4 vModelMatrix;

#include "../../GlobalUbo.glsl"

void main() {
  gl_Position = (vModelMatrix * vPosition) - vec4(CameraPos, 0.0);
}