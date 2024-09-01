#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"
#include "util/tbn.glsl"
#include "model/model_variant.glsl"

uniform float unSnowLevel;
uniform int unCrossfadeOffset = -1;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialSlot;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 6) in float vWindInfluence;
layout(location = 7) in mat4 vModelMatrix;
layout(location = 12) in uint vDamageZoneIndex;
layout(location = 13) in uvec3 vSubmeshIndexVariantIndexDamageModelIndex;

struct ModelDamageZoneGPUData {
    uint damageZones[8]; // Each uint holds four uint8_t values
    vec2 bottom;
    vec2 top;
    vec2 radii;
};

layout(std430, binding = 7) readonly restrict buffer ModelDamageZoneBuffer {
    ModelDamageZoneGPUData modelDamageZoneBuffer[];
};

layout(std430, binding = 9) readonly restrict buffer CrossfadeBuffer {
    float crossfadeBuffer[];
};


uint extractDamageZoneByte(ModelDamageZoneGPUData data, uint index) {
    if (index >= 32) {
        return 0; 
    }
    // Determine which uint in the array holds the byte
    uint uintIndex = index / 4;
    // Determine the byte's position within the uint
    uint byteIndex = index % 4;

    // Extract the byte
    uint value = data.damageZones[uintIndex];
    uint byte = (value >> (byteIndex * 8)) & 0xFF;
    return byte;
}

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;
out float fSnow;
out float fDamage;
out vec3 fLocalPosition;
flat out float fCrossfade;

float damageTest(inout vec4 position, float damageValue) {
    vec2 offset = position.xy;
    float len = length(offset);
    vec2 offsetNormalized = offset / len;
    len = max(len * (1.0 - damageValue), min(len * 0.3, 1.0));
    position.xy = offsetNormalized * len;
    return damageValue;
}

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    uint submeshOffset = vMaterialSlot / 4;
    int windType = inSubmeshWindData[vSubmeshIndexVariantIndexDamageModelIndex.x + submeshOffset];
    fMaterialIndex = inVariantMaterials[vSubmeshIndexVariantIndexDamageModelIndex.y + vMaterialSlot];

	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
	fTBN = computeTbn(mat3(vModelMatrix), normal, tangent);
    
    if (unCrossfadeOffset != -1) {
        fCrossfade = crossfadeBuffer[unCrossfadeOffset + gl_DrawID];
    } else {
        fCrossfade = -0.0001; // Indicates fully rendered object
    }
    
    fSnow = max(normal.z, 0.0) * unSnowLevel;

    
// TODO: Handle defines
//#ifndef SMUDGE
    
    vec4 adjustedPosition = vPosition;
  
    uint damageValue = extractDamageZoneByte(modelDamageZoneBuffer[vSubmeshIndexVariantIndexDamageModelIndex.z], vDamageZoneIndex);
    fDamage = float(damageValue) / 255.0;
    // TODO: Remove fDamage = 
    //damageTest(adjustedPosition, fDamage);
    
    fLocalPosition = adjustedPosition.xyz;
    
    // Snow
    adjustedPosition.z += fSnow * 0.25f;
    
//#endif
    
    vec4 trueWorldPos = (vModelMatrix * adjustedPosition);
    vec3 modelRoot = vModelMatrix[3].xyz;
    
    float height = adjustedPosition.z;
    
    float windPower = pow(1.0 - unSnowLevel, 4.0);
    addModelWind(trueWorldPos, modelRoot, windType, height * windPower);
    
    vec4 relativeWorldPos = trueWorldPos - vec4(CameraPos, 0.0);
    gl_Position = VP * relativeWorldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN); // Transpose is same as inverse for tbn because it is orthogonal, apparently
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * relativeWorldPos.xyz;
}