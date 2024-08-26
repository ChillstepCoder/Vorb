#include "GlobalUbo.glsl"


uniform vec3 unPosition;

uniform float UnYOffset = 1.0;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

// This must not be modified, it is bound to code layout
struct BillboardData {
   vec3 position;
   float xFlip;
   vec2 dims;
   uint materialIndex;
};

layout(std430, binding = 3) restrict readonly buffer BillboardSSBO {
    BillboardData billboardData[];
};

uniform uint unBaseInstanceOffset = 0;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;

vec2 getUvs(int idx, float xFlip) {
    vec2 uvMult = (VertexData[idx] + 1.0) * 0.5;
	vec4 uvAdjusted = vec4(0.0, 0.0, 1.0, 1.0);
    // Flip if needed
    uvAdjusted.x -= step(0.0, xFlip) * uvAdjusted.z;
    uvAdjusted.z *= -xFlip;
    return uvAdjusted.xy + uvAdjusted.zw * uvMult;
}

vec2 getVertexOffsets(int idx) {
    vec2 vertexOffsets = VertexData[idx];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

void main() {
    const int idx = indices[gl_VertexID % 6];
    const uint billboardId = unBaseInstanceOffset + (gl_VertexID / 6);

    BillboardData data = billboardData[billboardId];
    
	vec4 vPosition = vec4(data.position, 1.0);
	vec2 vDims = data.dims;
	
	// Compute uvs
    fUV = getUvs(idx, data.xFlip);
    fMaterialIndex = data.materialIndex;
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets(idx);
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims;
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

    fTint = vec4(1.0);
	
	// Hardcoded for facing up
	fTBN = mat3(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0));
    
    fRoughness = 0.9;
	
    gl_Position = glPos;
}