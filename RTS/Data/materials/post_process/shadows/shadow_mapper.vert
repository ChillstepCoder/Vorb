// Input
in vec3 vPosition; // Position in screen space
#include "../../GlobalUbo.glsl"

void main() {
  gl_Position =  vec4(vPosition - CameraPos, 1.0);
}