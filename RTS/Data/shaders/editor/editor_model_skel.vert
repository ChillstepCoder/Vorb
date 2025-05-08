#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"
#include "model/model_variant.glsl"

uniform mat4 unM;
uniform mat4 unVP;
uniform vec4 unPosOffset = vec4(0.0);
uniform vec3 unCameraPos;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialSlot;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 7) in mat4 vModelMatrix;
layout(location = 11) in vec4 vBoneWeights;
layout(location = 12) in ivec4 vBoneIds;

out vec2 fUV;
out vec3 fWorldPos;
out vec2 fScreenPos;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fTangent;
out vec3 fNormal;
out vec3 fViewTangent;
out vec3 fFragPosTangent;

uniform int unVariantIndex;

uniform vec3 unOffset;
const int MAX_BONES = 100;
uniform mat4 unBoneTransforms[MAX_BONES];

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    
    // TODO: Do in fragment shader?
    fMaterialIndex = inVariantMaterials[unVariantIndex + vMaterialSlot];
    
    mat4 boneTransform = unBoneTransforms[vBoneIds[0]] * vBoneWeights[0];
    boneTransform += unBoneTransforms[vBoneIds[1]] * vBoneWeights[1];
    boneTransform += unBoneTransforms[vBoneIds[2]] * vBoneWeights[2];
    boneTransform += unBoneTransforms[vBoneIds[3]] * vBoneWeights[3];
    
    vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    vec4 localPos = boneTransform * vec4(vPosition.xyz, 1.0);
    vec4 localNormal = boneTransform * vec4(normal, 0.0);
    vec4 localTangent = boneTransform * vec4(tangent, 0.0);

    vec4 transformedPos = unM * localPos;
    mat3 modelMatrix3 = mat3(unM);
    
    localNormal.xyz = modelMatrix3 * localNormal.xyz;
    localTangent.xyz = modelMatrix3 * tangent.xyz;
  
    vec3 bitangent = cross(localNormal.xyz, localTangent.xyz);
    fTBN = mat3(localTangent.xyz, bitangent, localNormal.xyz);
    
    fNormal = normal;
    // For debugging
    fTangent = localTangent.xyz;
    
    vec4 worldPos = transformedPos + unPosOffset;
    fWorldPos = worldPos.xyz;
    vec4 screenPos = unVP * worldPos;
    gl_Position = screenPos;
    // Homogenous space to NDC
    fScreenPos = ((screenPos.xy / screenPos.w) + 1.0) * 0.5;
    
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = inverse(fTBN);
    fViewTangent  = tfTBN * unCameraPos;
    fFragPosTangent  = tfTBN * fWorldPos;
    
}