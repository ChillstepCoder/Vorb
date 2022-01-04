
#include "../GlobalUbo.glsl"

uniform samplerBuffer UnTboPosition;
uniform samplerBuffer UnTboSizeType;
uniform vec3 unOffset;
uniform float UnYOffset = 1.0;

out vec3 fWorldPos;
flat out vec3 fWorldRoot;
out float fHeight;
out vec2 fScreenUV;
out vec2 fUV;
flat out float fAtlasPage;
out mat3 fTBN;
out float fRoughness; // TODO: GLOBAL
out float fDistance;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

// TODO: Match number of grass types
const int GRASS_TYPES = 4;
const float GRASS_UV_X = 0.25; // 1 / 4
const vec2 UVS[8] = {
 // NORMAL UVS
 {0.0, 0.0 },
 {GRASS_UV_X, 0.0 },
 {GRASS_UV_X, 1.0 },
 {0.0, 1.0 },
 // INVERT UVS AFTER HERE
 {GRASS_UV_X, 0.0 },
 {0.0, 0.0 },
 {0.0, 1.0 },
 {GRASS_UV_X, 1.0 }
};

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

void main() {
    int bladeIndex = (gl_VertexID / 4);
	vec4 vPosition = vec4(texelFetch(UnTboPosition, bladeIndex).rgb, 1.0);
	vec3 dimsType = texelFetch(UnTboSizeType, bladeIndex).rgb;
	vec2 vDims = dimsType.xy;
    float bladeType = round(dimsType.z * 255.0);
	
	// Get uniform info
	fRoughness = 0.0;
	
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
    fWorldRoot = windPos;
    fHeight = vertexOffsets.y;
    //worldPos.xyz += CameraRight * getWindAtPosition(-Time, vec4(windPos, 0.0)) * vertexOffsets.y;
	
	vec4 glPos = VP * worldPos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	// Lean away at top
	vec3 glPosNoX = vec3(0.0, min(glPos.y, -1.0), glPos.z);
	float angle = 1.0 - dot(screenCamera.xyz, normalize(glPosNoX));
	
	float distanceFromCamera = clamp((200.0 - glPos.z) * 0.01, 0.0, 1.0); // Make so far away doesnt lean
	angle = min(pow(angle, 0.4) * xzOffsetUncompressed.y, 1.0) * distanceFromCamera;
	fDistance = length(worldPos.xy);
	
	worldPos.xyz += CameraFront * angle;
    fWorldPos = worldPos.xyz;
	vec4 screenPos = VP * worldPos;
	//gl_Position = screenPos; // NO DO FOR TESSELATION
	
	// Compute uvs as screen coords
    fScreenUV = (screenPos.xy / vec2(screenPos.w));
	
	// Grass blade uvs
	fUV = UVS[gl_VertexID % 4 + (4 * (bladeIndex % 2))];
    fUV.x += bladeType * 0.25;

	// Hardcoded for facing up
	fTBN = mat3(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
}