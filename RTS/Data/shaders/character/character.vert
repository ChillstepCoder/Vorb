#include "GlobalUbo.glsl"
#include "util/uv.glsl"
#include "model/model_variant.glsl"

const uint MATERIAL_SLOT_COUNT = 4;

// Input
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialSlot;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
// 6 is reserved for wind influence
// 7-10 Is reserved for model matrices but we use the SSBO below
layout(location = 11) in vec4 vBoneWeights;
layout(location = 12) in ivec4 vBoneIds;
layout(location = 13) in uvec3 vModelTransformIndexBoneTransformIndexVariantIndex;

out vec4 fTint;
out vec2 fUV;
flat out uint fMaterialIndex;
out mat3 fTBN;
out vec3 fNormal;

layout(std430, binding = 6) readonly buffer SkinningMatrices {
    mat4 inSkinningMatrices[];
};

layout(std430, binding = 8) readonly buffer ModelTransforms {
    mat4 inModelTransforms[];
};


void main() {
  fTint = vTint;
  fUV = unpackUV(vUV);
  // Unlike static models we recieve the exact position for this submesh so we use % MATERIAL_SLOT_COUNT as vMaterialSlot is relative to
  // the first submesh in the model
  fMaterialIndex = inVariantMaterials[vModelTransformIndexBoneTransformIndexVariantIndex.z + vMaterialSlot % MATERIAL_SLOT_COUNT];
  
  mat4 boneTransform = inSkinningMatrices[vModelTransformIndexBoneTransformIndexVariantIndex.y + vBoneIds[0]] * vBoneWeights[0];
  boneTransform += inSkinningMatrices[vModelTransformIndexBoneTransformIndexVariantIndex.y + vBoneIds[1]] * vBoneWeights[1];
  boneTransform += inSkinningMatrices[vModelTransformIndexBoneTransformIndexVariantIndex.y + vBoneIds[2]] * vBoneWeights[2];
  boneTransform += inSkinningMatrices[vModelTransformIndexBoneTransformIndexVariantIndex.y + vBoneIds[3]] * vBoneWeights[3];
  
  mat4 modelTransform = inModelTransforms[vModelTransformIndexBoneTransformIndexVariantIndex.x];
  
  vec3 normal = normalize(vNormal);
  vec3 tangent = normalize(vTangent);
  
  vec4 localPos = boneTransform * vec4(vPosition.xyz, 1.0);
  
  // TODO: Should this be a 3x3?
  vec4 localNormal = boneTransform * vec4(normal, 0.0);
  vec4 localTangent = boneTransform * vec4(tangent, 0.0);

  vec4 transformedPos = modelTransform * localPos;
  gl_Position = VP * transformedPos;
  
  
  localNormal = normalize(modelTransform * localNormal);
  localTangent = normalize(modelTransform * localTangent);
  
  //https://learnopengl.com/Advanced-Lighting/Normal-Mapping
  // re-orthogonalize T with respect to N
  //localTangent = normalize(localTangent - dot(localTangent, localNormal) * localNormal);

  vec3 bitangent = cross(localNormal.xyz, localTangent.xyz);
  fTBN = mat3(localTangent.xyz, bitangent, localNormal.xyz);
}