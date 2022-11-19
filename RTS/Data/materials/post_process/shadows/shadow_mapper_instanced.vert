// Input
layout(location = 0) in vec3 vPosition; // Position in world space
layout(location = 7) in vec3 iPosition;

#include "../../GlobalUbo.glsl"

void main() {
  gl_Position = vec4(vPosition + (iPosition - CameraPos), 1.0);
}