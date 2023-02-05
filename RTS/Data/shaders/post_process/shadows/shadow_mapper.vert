// Input
layout(location = 0) in vec3 vPosition; // Position in world space
#include "../../GlobalUbo.glsl"

uniform vec3 unPosition;

void main() {
  gl_Position = vec4(unPosition, 0.0) + vec4(vPosition - CameraPos, 1.0);
}