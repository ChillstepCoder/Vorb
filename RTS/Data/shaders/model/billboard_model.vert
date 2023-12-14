#include "GlobalUbo.glsl"

uniform float UnYOffset = 1.0;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

// This must not be modified, it is bound to code layout
struct BillboardData {
   vec3 position;
   vec2 dims;
   uint material;
};

layout(std430, binding = 3) restrict readonly buffer BillboardSSBO {
    BillboardData billboardData[];
};


uniform vec3 unPosition;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;


BillboardData getBillboardData() {
  return billboardData[(gl_VertexID / 4)];
}

vec2 getUvs() {
    return (VertexData[gl_VertexID % 4] + 1.0) * 0.5;
}

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

void main() {

    BillboardData data = getBillboardData();
    
	vec4 vPosition = vec4(data.position, 1.0);
	vec2 vDims = data.dims;
	
	// Compute uvs
    fUV = getUvs();
    fMaterialIndex = data.material;
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	
	vec4 worldPos = vertexPosition + vec4(unPosition - CameraPos, 0.0);
    
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