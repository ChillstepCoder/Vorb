#include "GlobalUbo.glsl"
#include "util/uv.glsl"

// Input
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialIndex;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec2 vTangent;
// 6 is reserved for wind influence
layout(location = 7) in mat4 vModelMatrix;
// Model matrix consumes 4 locations
layout(location = 11) in vec4 vBoneWeights;
layout(location = 12) in ivec4 vBoneIds;

out vec4 fTint;
out vec2 fUV;
flat out uint fMaterialIndex;
out mat3 fTBN;
out vec3 fNormal;

uniform vec3 unOffset;
uniform mat4 unModelTransform;
const int MAX_BONES = 100;
uniform mat4 unBoneTransforms[MAX_BONES];

void main() {
  fTint = vTint;
  fUV = unpackUV(vUV);
  fMaterialIndex = vMaterialIndex;
  
  mat4 boneTransform = unBoneTransforms[vBoneIds[0]] * vBoneWeights[0];
  boneTransform += unBoneTransforms[vBoneIds[1]] * vBoneWeights[1];
  boneTransform += unBoneTransforms[vBoneIds[2]] * vBoneWeights[2];
  boneTransform += unBoneTransforms[vBoneIds[3]] * vBoneWeights[3];
  
  
  vec3 normal = normalize(vNormal);
  vec3 tangent = normalize(vec3(vTangent, 0));
  vec4 localPos = boneTransform * vec4(vPosition.xyz, 1.0);
  vec4 localNormal = boneTransform * vec4(normal, 0.0);
  vec4 localTangent = boneTransform * vec4(tangent, 0.0);

  vec4 transformedPos = unModelTransform * localPos;
  vec4 worldPos = transformedPos + vec4(unOffset, 0.0);
  gl_Position = VP * worldPos;
  
  
  localNormal = (unModelTransform * localNormal);
  localTangent = (unModelTransform * localTangent);
  

  vec3 bitangent = cross(localNormal.xyz, localTangent.xyz);
  fTBN = mat3(localTangent.xyz, bitangent, localNormal.xyz);
}