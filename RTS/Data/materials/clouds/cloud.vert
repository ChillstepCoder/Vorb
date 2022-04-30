#include "BillboardSSBO.glsl"
#include "GlobalUbo.glsl"

uniform vec3 UnRootPos;

// TODO: Don't use the altas, shrink the coordinates
out vec2 fUV;
out vec2 fPosition;
flat out int fTextureIndex;
out vec4 fTint;
out float fRoughness;
out mat3 fTBN;

uniform float DebugFloat1;
uniform float DebugFloat2;

BillboardData getBillboardData() {
  return billboardData[(gl_VertexID / 4)];
}

vec2 getUvsFromTextureIndex(int textureIndex, float xFlip) {
    vec2 uvMult = (VertexData[gl_VertexID % 4] + 1.0) * 0.5;
	vec4 vUV = vec4(0.0, 0.0, 1.0, 1.0);
	vec4 uvAdjusted = vUV;
    // TODO: Why?
	uvAdjusted.w = -uvAdjusted.w;
	uvAdjusted.y -= uvAdjusted.w;
    // Flip if needed
    uvAdjusted.x -= step(0.0, xFlip) * uvAdjusted.z;
    uvAdjusted.z *= -xFlip;
    return uvAdjusted.xy + uvAdjusted.zw * uvMult;
}

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

vec3 rotateXY(vec3 inVec, float angle) {
    vec3 rv;
    float cosa = cos(angle);
    float sina = sin(angle);
    rv.x = cosa * inVec.x - sina * inVec.y;
    rv.y = sina * inVec.x + cosa * inVec.y;
    rv.z = inVec.z;
    
    return rv;
}

// https://www.neilmendoza.com/glsl-rotation-about-an-arbitrary-axis/
mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main() {

    BillboardData data = getBillboardData();
    
	// Compute uvs
    fUV = getUvsFromTextureIndex(data.texture, data.xFlip);

	vec4 vPosition = vec4(data.position, 1.0);
	vec2 vDims = data.dims;
    fTextureIndex = data.texture;
	
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
    
    // Movement offset
	vertexPosition.xyz += cos((vPosition.x + UnRootPos.x) * 0.05 - sin((vPosition.y + UnRootPos.y) * 0.05)) * 10.0;
    
    // Get Right and Up vectors in world space
    vec3 cameraNormal = vertexPosition.xyz + UnRootPos - CameraPos;
    cameraNormal = normalize(cameraNormal);
    vec3 worldRight = normalize(vec3(rotateXY(cameraNormal, -90.0 * (3.141592653 / 180.0)).xy, 0.0));
    vec3 worldUp = normalize((rotationMatrix(worldRight, -90.0) * vec4(cameraNormal, 1.0)).xyz);
    
	vertexPosition.xyz += worldUp * xzOffsetUncompressed.y;
	vertexPosition.xyz += worldRight * xzOffsetUncompressed.x;
    
    vec3 normal = -cameraNormal; // Prenormalized on CPU
	vec3 binormal = -worldUp;
    vec3 tangent = worldRight;
	fTBN = mat3(tangent, binormal, normal);

    fTint = vec4(1.0);
	
	vec4 worldPos = vertexPosition + vec4(UnRootPos - CameraPos, 0.0);

	//fTint.r = 1.0 - angle;
	//fTint.g = 0.0;
	//fTint.b = 0.0;
	
	vec4 glPos = VP * worldPos;
    gl_Position = glPos;
	
}