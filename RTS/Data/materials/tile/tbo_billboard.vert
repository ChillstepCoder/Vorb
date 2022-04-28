
#include "../GlobalUbo.glsl"
#include "../TboBillboardShared.glsl"

uniform vec3 unOffset;

out vec2 fUV;
flat out int fTextureIndex;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;

#include "../util/wind.glsl"

void main() {

	vec4 vPosition = vec4(getPositionFromTbo(), 1.0);
	vec3 typeSize = getTypeSizeFromTbo();
	vec2 vDims = typeSize.yz;
	int type = int(typeSize.x);
	
	// Get uniform info
	vec3 atlasPageRoughnessWind = UnAtlasPageRoughnessWind[type].rgb;
	fRoughness = atlasPageRoughnessWind.g;
	float vWindInfluence = atlasPageRoughnessWind.b;
	
	// Compute uvs
    fUV = getUvsFromType(type);
    fTextureIndex = int(atlasPageRoughnessWind.r);
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	
	vec4 worldPos = vertexPosition + vec4(unOffset, 0.0);
    
    // Wind
    worldPos.xyz += CameraRight * getWindAtPosition(Time, vPosition) * vWindInfluence * vertexOffsets.y;
	
	vec4 glPos = VP * worldPos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	// Lean away at top
	vec3 glPosNoX = vec3(0.0, min(glPos.y, -1.0), glPos.z);
	float angle = 1.0 - dot(screenCamera.xyz, normalize(glPosNoX));
	
	float distanceFromCamera = clamp((200.0 - glPos.z) * 0.01, 0.0, 1.0); // Make so far away doesnt lean
	angle = min(pow(angle, 0.4) * xzOffsetUncompressed.y, 1.0) * distanceFromCamera;
	
	worldPos.xyz += CameraFront * angle;
	glPos = VP * worldPos;

    fTint = vec4(1.0);
	
	// Hardcoded for facing the camera
	//fTBN = mat3(-CameraUp, -CameraRight, -CameraFront);
	// Hardcoded for facing up
	fTBN = mat3(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
	
	
    gl_Position = glPos;
}