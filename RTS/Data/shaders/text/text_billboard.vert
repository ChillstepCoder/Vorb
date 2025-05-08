#include "TextSSBO.glsl"
#include "GlobalUbo.glsl"

uniform vec3 unOffset;

out vec2 fUV;

GlyphData getGlyphData() {
  return glyphData[(gl_VertexID / 4)];
}

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets.x += 1.0;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

vec2 getUvs(GlyphData data) {
    vec2 uvMult = (VertexData[gl_VertexID % 4] + 1.0);
	vec4 uvAdjusted = data.uvs;
    // Invert z
	uvAdjusted.w = -uvAdjusted.w;
	uvAdjusted.y -= uvAdjusted.w;
    // Flip if needed
    //vAdjusted.x -= step(0.0, xFlip) * uvAdjusted.z;
    //uvAdjusted.z *= -xFlip;
    return uvAdjusted.xy + uvAdjusted.zw * uvMult * 0.5;
}

void main() {
    GlyphData data = getGlyphData();
    
	vec4 vPosition = vec4(data.origin, 1.0);
	vec2 vDims = data.dims;
	
	// Compute uvs
    fUV = getUvs(data);
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.xyz += CameraRight * (xzOffsetUncompressed.x + data.xyOffset.x) + CameraUp * (xzOffsetUncompressed.y + data.xyOffset.y);
	
	vec4 worldPos = vertexPosition + vec4(unOffset, 0.0);
    
	vec4 glPos = VP * worldPos;
	
    gl_Position = VP * worldPos;
}