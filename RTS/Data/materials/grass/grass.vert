
#include "../GlobalUbo.glsl"

uniform samplerBuffer UnTboPosition;
uniform samplerBuffer UnTboColorSize;
uniform vec3 unOffset;
uniform float UnYOffset = 1.0;

out vec2 fUV;
flat out float fAtlasPage;
out vec3 fTint;
out mat3 fTBN;
out float fRoughness;

#include "../util/wind.glsl"

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

void main() {
    int posIndex = (gl_VertexID / 4);
    int colorSizeIndex = posIndex * 2;
	vec4 vPosition = vec4(texelFetch(UnTboPosition, posIndex).rgb, 1.0);
	fTint = texelFetch(UnTboColorSize, colorSizeIndex).rgb;
	vec2 vDims = texelFetch(UnTboColorSize, colorSizeIndex + 1).rg;
	
	// Get uniform info
	fRoughness = 0.0;
	float vWindInfluence = 1.0;
	
	// Compute uvs
    fUV = vec2(1.0);
    fAtlasPage = 0.0;
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	
	vec4 worldPos = vertexPosition + vec4(unOffset, 0.0);
    
    // Wind
	vec3 windPos = vPosition.xyz + unOffset + CameraPos;
    worldPos.xyz += CameraRight * getWindAtPosition(-Time, vec4(windPos, 0.0)) * vWindInfluence * vertexOffsets.y;
	
	vec4 glPos = VP * worldPos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	// Lean away at top
	vec3 glPosNoX = vec3(0.0, min(glPos.y, -1.0), glPos.z);
	float angle = 1.0 - dot(screenCamera.xyz, normalize(glPosNoX));
	
	float distanceFromCamera = clamp((200.0 - glPos.z) * 0.01, 0.0, 1.0); // Make so far away doesnt lean
	angle = min(pow(angle, 0.4) * xzOffsetUncompressed.y, 1.0) * distanceFromCamera;
	
	worldPos.xyz += CameraFront * angle;
	glPos = VP * worldPos;

	// Hardcoded for facing up
	fTBN = mat3(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
	
	
    gl_Position = glPos;
}