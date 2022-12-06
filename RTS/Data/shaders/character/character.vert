#include "../GlobalUbo.glsl"

// Input
in vec4 vPosition; // Position in screen space
in vec4 vTint;
in vec3 vNormal;
in vec3 vTangent;
in vec2 vUV;
in ivec4 vBoneIds;
in vec4 vBoneWeights;

out vec4 fTint;
out vec2 fUV;
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
  
  mat4 boneTransform = unBoneTransforms[vBoneIds[0]] * vBoneWeights[0];
  boneTransform += unBoneTransforms[vBoneIds[1]] * vBoneWeights[1];
  boneTransform += unBoneTransforms[vBoneIds[2]] * vBoneWeights[2];
  boneTransform += unBoneTransforms[vBoneIds[3]] * vBoneWeights[3];
  
  vec4 localPos = boneTransform * vec4(vPosition.xyz, 1.0);

  vec4 scaledPos = vec4(localPos.xyz * unScale, 1.0);
  vec4 transformedPos = unModelTransform * scaledPos;
  vec4 worldPos = transformedPos + vec4(unOffset, 0.0);
  gl_Position = VP * worldPos;
  
  vec3 normal = (unModelTransform * vec4(vNormal, 1.0)).rgb;
  vec3 tangent = (unModelTransform * vec4(vTangent, 1.0)).rgb;
  //vec3 bitangent = (unModelTransform * vec4(vBitangent, 1.0)).rgb;
  
  vec3 bitangent = cross(normal, tangent);
  fTBN = mat3(tangent, bitangent, normal);
}