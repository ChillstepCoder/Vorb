// Input
in vec3 vPosition; // Position in world space
#include "../../GlobalUbo.glsl"

uniform vec3 unOffset;

void main() {
  gl_Position = vec4(vPosition + unOffset, 1.0);
}