uniform samplerBuffer UnTboPositionTypeSize;
uniform float UnYOffset = 1.0;

// UBO
layout (std140, binding = 1) uniform TboBillboardData
{
  vec4 UnUvs[256];
  vec4 UnAtlasPageRoughnessWind[256];
};

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

vec3 getPositionFromTbo() {
  int index = (gl_VertexID / 4) * 2;
  return texelFetch(UnTboPositionTypeSize, index).rgb;
}
vec3 getTypeSizeFromTbo() {
  int index = (gl_VertexID / 4) * 2;
  return texelFetch(UnTboPositionTypeSize, index + 1).rgb;
}

vec2 getUvsFromType(int type) {
    vec2 uvMult = (VertexData[gl_VertexID % 4] + 1.0) * 0.5;
	vec4 vUV = UnUvs[type];
	vec4 uvAdjusted = vUV;
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