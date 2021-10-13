// Input
in vec4 vPosition;
in vec2 vUV;

uniform mat4 VP;
uniform mat4 SkyRotMatrix;

out vec2 fUV;
out vec3 fPosition;

void main() {
  fUV = vUV;
  fPosition = normalize(vPosition.xyz);
  gl_Position = VP * SkyRotMatrix * vPosition;
}