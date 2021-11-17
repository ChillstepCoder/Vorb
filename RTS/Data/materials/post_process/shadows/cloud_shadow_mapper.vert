// Input
in vec3 vPosition; // Position in screen space
in vec2 vXZOffset;
in vec2 vUV;
in float vAtlasPage;

uniform vec3 SunUp;
uniform vec3 SunRight;
uniform vec3 CameraPos;

out vec2 gUV;
flat out float gAtlasPage;

void main() {
  vec3 vertexPosition = vPosition;
  vec2 xzOffsetUncompressed = vXZOffset / 100.0; // Matches C++ compression ratio
  vertexPosition += SunUp * xzOffsetUncompressed.y;
  vertexPosition += SunRight * xzOffsetUncompressed.x;
  vec3 position = vec3(vPosition.x + vXZOffset.x, vPosition.y + vXZOffset.y, vPosition.z);
  gl_Position = vec4(vertexPosition - CameraPos, 1.0);
  
  gUV = vUV;
  gAtlasPage = vAtlasPage;
}