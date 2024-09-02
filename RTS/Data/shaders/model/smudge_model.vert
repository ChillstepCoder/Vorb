#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"
#include "util/tbn.glsl"
#include "model/model_variant.glsl"

uniform float unSnowLevel;
uniform int unCrossfadeEnabled = 0;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialSlot;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 6) in float vWindInfluence;
layout(location = 7) in mat4 vModelMatrix;
layout(location = 13) in uvec3 vSubmeshIndexVariantIndexDamageModelIndex;

layout(std430, binding = 9) readonly restrict buffer CrossfadeBuffer {
    float crossfadeBuffer[];
};


out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;
flat out float fCrossfade;

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    uint submeshOffset = vMaterialSlot / 4;
    int windType = inSubmeshWindData[vSubmeshIndexVariantIndexDamageModelIndex.x + submeshOffset];
    fMaterialIndex = inVariantMaterials[vSubmeshIndexVariantIndexDamageModelIndex.y + vMaterialSlot];
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
    fTBN = computeTbn(mat3(vModelMatrix), normal, tangent);
    
    mat3 modelMatrix3 = mat3(vModelMatrix);
    normal = modelMatrix3 * normal;
    tangent = modelMatrix3 * tangent;
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
    
    if (unCrossfadeEnabled == 1) {
        fCrossfade = crossfadeBuffer[gl_DrawID];
    } else {
        fCrossfade = -0.0001; // Indicates fully rendered object
    }
    
    vec4 adjustedPosition = vPosition;
    
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