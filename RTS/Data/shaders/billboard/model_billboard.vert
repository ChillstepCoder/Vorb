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
   float crossfade;
};

layout(std430, binding = 3) restrict readonly buffer BillboardSSBO {
    BillboardData billboardData[];
};

out vec2 fUV;
flat out uint fMaterialIndex;
out mat3 fTBN;
flat out float fCrossfade;

vec2 flipUV(vec2 uv, float uFlip) {
    float flippedU = uFlip * (1.0 - uv.x) + (1.0 - uFlip) * uv.x;
    return vec2(flippedU, 1.0 - uv.y);
}

vec2 getUvs(int idx, float uFlip) {
    return flipUV((VertexData[idx] + 1.0) * 0.5, uFlip);
}

vec2 getVertexOffsets(int idx) {
    vec2 vertexOffsets = VertexData[idx];
	vertexOffsets.y += UnYOffset;
	return vertexOffsets;
}

mat3 computeBillboardTBN(vec3 CameraRelativePos) {
    // Compute the view direction (from billboard to camera)
    vec3 N = normalize(-CameraRelativePos);
    
    // Ensure the normal is not parallel to the up vector
    if (abs(N.z) > 0.999999) {
        N = vec3(0.000001, 0, N.z);
    }
    
    // Compute the right vector
    vec3 T = normalize(cross(vec3(0, 0, 1), N));
    
    // Compute the up vector
    vec3 B = cross(N, T);
    
    // Construct the TBN matrix
    return mat3(T, B, N);
}

void main() {
    const int idx = indices[gl_VertexID % 6];
    const uint billboardId = (gl_VertexID / 6);

    BillboardData data = billboardData[billboardId];
    
    fCrossfade = data.crossfade;
    
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
	
	// Hardcoded for facing up
	fTBN = computeBillboardTBN(worldPos.xyz);
    
    gl_Position = glPos;
}