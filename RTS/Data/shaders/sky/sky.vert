// Input
in vec4 vPosition;
in vec2 vUV;

#include "../GlobalUbo.glsl"

uniform mat4 SkyRotMatrix;

out vec2 fUV;
out vec3 fPosition;
out vec3 fSkyVector;

void main() {
  fUV = vUV;
  fPosition = normalize(vPosition.xyz);
  fSkyVector = normalize((SkyRotMatrix * vPosition).xyz);
  gl_Position = VP * SkyRotMatrix * vPosition;
}