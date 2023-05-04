// Input
in vec4 vPosition;
in vec2 vUV;

#include "../GlobalUbo.glsl"
#include "util/uv.glsl"

uniform mat4 unVP;
uniform mat4 SkyRotMatrix;

out vec2 fUV;
out vec3 fPosition;
out vec3 fSkyVector;

void main() {
  fUV = unpackUV(vUV);
  fPosition = normalize(vPosition.xyz);
  fSkyVector = normalize((SkyRotMatrix * vPosition).xyz);
  vec4 pos = unVP * SkyRotMatrix * vPosition;
  gl_Position = pos.xyww; // Force depth to 1.0
}