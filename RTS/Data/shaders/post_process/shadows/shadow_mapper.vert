// Input
layout(location = 0) in vec3 vPosition; // Position in world space
#include "../../GlobalUbo.glsl"

void main() {
  gl_Position = vec4(vPosition - CameraPos, 1.0);
}