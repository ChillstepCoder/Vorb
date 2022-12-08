#include "../GlobalUbo.glsl"

// Input
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialIndex;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec2 vTangent;
layout(location = 6) in vec4 vBoneWeights;
layout(location = 7) in ivec4 vBoneIds;

out vec4 fTint;
out vec2 fUV;
flat out uint fMaterialIndex;
out mat3 fTBN;
out vec3 fNormal;

uniform vec3 unOffset;
uniform float unScale;
uniform mat4 unModelTransform;
const int MAX_BONES = 100;
uniform mat4 unBoneTransforms[MAX_BONES];

void main() {
  fTint = vTint;
  fUV = vUV;
  fMaterialIndex = vMaterialIndex;
  
  mat4 boneTransform = unBoneTransforms[vBoneIds[0]] * vBoneWeights[0];
  boneTransform += unBoneTransforms[vBoneIds[1]] * vBoneWeights[1];
  boneTransform += unBoneTransforms[vBoneIds[2]] * vBoneWeights[2];
  boneTransform += unBoneTransforms[vBoneIds[3]] * vBoneWeights[3];
  
  vec4 localPos = boneTransform * vec4(vPosition.xyz, 1.0);

  vec4 scaledPos = vec4(localPos.xyz * unScale, 1.0);
  vec4 transformedPos = unModelTransform * scaledPos;
  vec4 worldPos = transformedPos + vec4(unOffset, 0.0);
  gl_Position = VP * worldPos;
  
  
  vec3 normal = normalize(vNormal);
  vec3 tangent = normalize(vec3(vTangent, 0));
  vec3 binormal = cross(normal, tangent);
  normal = (unModelTransform * vec4(normal, 1.0)).rgb;
  tangent = (unModelTransform * vec4(tangent, 1.0)).rgb;
  //vec3 bitangent = (unModelTransform * vec4(vBitangent, 1.0)).rgb;
  

  vec3 bitangent = cross(normal, tangent);
  fTBN = mat3(tangent, bitangent, normal);
}