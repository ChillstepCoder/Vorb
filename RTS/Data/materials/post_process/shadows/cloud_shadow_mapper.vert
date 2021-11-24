// Input
in vec3 vPosition; // Position in screen space
in vec2 vXZOffset;
in vec2 vUV;
in float vAtlasPage;

#include "../../GlobalUbo.glsl"
#include "../../TboBillboardShared.glsl"

out vec2 gUV;
flat out float gAtlasPage;

void main() {
  vec4 vPosition = vec4(getPositionFromTbo(), 1.0);
  vec3 typeSize = getTypeSizeFromTbo();
  vec2 vDims = typeSize.yz;
  int type = int(typeSize.x);
  
  vec2 vertexOffsets = getVertexOffsets();
  vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
  vec3 vertexPosition = vPosition.xyz;
  vertexPosition += SunUp * xzOffsetUncompressed.y;
  vertexPosition += SunRight * xzOffsetUncompressed.x;
  vertexPosition -= SunPosition * vDims.x * 0.5;
  gl_Position = vec4(vertexPosition - CameraPos, 1.0);
  
  gUV = getUvsFromType(type);
  gAtlasPage = UnAtlasPageRoughnessWind[type].r;
}