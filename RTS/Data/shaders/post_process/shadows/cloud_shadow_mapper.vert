#include "BillboardSSBO.glsl"
#include "GlobalUBO.glsl"
// Input
in vec3 vPosition; // Position in screen space
in vec2 vXZOffset;
in vec2 vUV;
in float vAtlasPage;

BillboardData getBillboardData() {
  return billboardData[(gl_VertexID / 4)];
}

vec2 getUvsFromTextureIndex(int textureIndex) {
    vec2 uvMult = (VertexData[gl_VertexID % 4] + 1.0) * 0.5;
	vec4 vUV = typeData[textureIndex].uvs;
	vec4 uvAdjusted = vUV;
    // TODO: Why?
	uvAdjusted.w = -uvAdjusted.w;
	uvAdjusted.y -= uvAdjusted.w;
    return uvAdjusted.xy + uvAdjusted.zw * uvMult;
}

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

uniform vec3 UnRootPos;

out vec2 gUV;
flat out int gTextureIndex;

void main() {
  BillboardData data = getBillboardData();

  // Compute uvs
  gUV = getUvsFromTextureIndex(data.texture);

  vec4 vPosition = vec4(data.position, 1.0);
  vec2 vDims = data.dims;
  gTextureIndex = data.texture;

  vec2 vertexOffsets = getVertexOffsets();
  vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
  vec3 vertexPosition = vPosition.xyz;
  vertexPosition += SunUp * xzOffsetUncompressed.y;
  vertexPosition += SunRight * xzOffsetUncompressed.x;
  //vertexPosition -= SunPosition * vDims.x * 0.5; 
  gl_Position = vec4(vertexPosition + (UnRootPos - CameraPos), 1.0);
  
}