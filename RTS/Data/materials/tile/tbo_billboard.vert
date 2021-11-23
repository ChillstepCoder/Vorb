uniform mat4 VP;
uniform float Time;
uniform vec3 CameraRight;
uniform vec3 CameraFront;
uniform vec3 CameraUp;
uniform vec3 CameraPos;
uniform samplerBuffer UnTboPositionTypeSize;

// UBO
layout (std140, binding = 1) uniform TboBillboardData
{
  vec4 UnUvs[256];
  vec4 UnAtlasPageRoughnessWind[256];
};

out vec2 fUV;
flat out float fAtlasPage;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;

#include "../util/wind.glsl"

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};


void main() {
    int index = gl_VertexID % 4;
	int tboIndex = gl_VertexID / 4;
	
	vec4 rootPositionTypeSize = texelFetch(UnTboPositionTypeSize, tboIndex).rgba;
	vec4 vPosition = vec4(rootPositionTypeSize.rgb, 1.0);
	float typeSize = rootPositionTypeSize.a;
	float size = mod(typeSize, 1000);
	int type = int(round((typeSize - size) / 1000.0));
	vec2 vDims = vec2(size);
	
	
	// Get uniform info
	vec4 vUV = UnUvs[type];
	vec3 atlasPageRoughnessWind = UnAtlasPageRoughnessWind[type].rgb;
	float vAtlasPage = atlasPageRoughnessWind.r;
	fRoughness = atlasPageRoughnessWind.g;
	float vWindInfluence = atlasPageRoughnessWind.b;
	
	vec2 vVertexData = VertexData[index];
    fTint = vec4(1.0);
	
	// Adjust test
	vec2 adjustedVertexData = vVertexData;
	adjustedVertexData.y += 1.0;
	adjustedVertexData *= 0.5;
	
	// Compute uvs
	vec2 uvMult = (vVertexData + 1.0) * 0.5;
	vec4 uvAdjusted = vUV;
	uvAdjusted.w = -uvAdjusted.w;
	uvAdjusted.y -= uvAdjusted.w;
    fUV = uvAdjusted.xy + uvAdjusted.zw * uvMult;
    fAtlasPage = vAtlasPage;
	
	// Compute position
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = adjustedVertexData * (vDims); // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	
	vec4 worldPos = vertexPosition - vec4(CameraPos, 0.0);
    
    // Wind
    worldPos.xyz += CameraRight * getWindAtPosition(Time, vPosition) * vWindInfluence * adjustedVertexData.y;
	
	vec4 glPos = VP * worldPos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	// Lean away at top
	vec3 glPosNoX = vec3(0.0, min(glPos.y, -1.0), glPos.z);
	float angle = 1.0 - dot(screenCamera.xyz, normalize(glPosNoX));
	
	float distanceFromCamera = clamp((200.0 - glPos.z) * 0.01, 0.0, 1.0); // Make so far away doesnt lean
	angle = min(pow(angle, 0.4) * xzOffsetUncompressed.y, 1.0) * distanceFromCamera;
	
	worldPos.xyz += CameraFront * angle;
	glPos = VP * worldPos;
	
	//fTint.r = 1.0 - angle;
	//fTint.g = 0.0;
	//fTint.b = 0.0;
	
	// Hardcoded for facing the camera
	//fTBN = mat3(-CameraUp, -CameraRight, -CameraFront);
	// Hardcoded for facing up
	fTBN = mat3(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
	
	
    gl_Position = glPos;
}