// Input
in vec4 vPosition;
in vec2 vUV;

uniform mat4 VP;
uniform mat4 SkyRotMatrix;

out vec2 fUV;
out vec3 fPosition;

void main() {
  fUV = vUV;
  vec3 relPosition =  vPosition.xyz - CameraPos;
  fPosition = normalize(relPosition);
  gl_Position = VP * SkyRotMatrix * vec4(relPosition, 1.0);
}