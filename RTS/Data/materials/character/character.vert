#include "../GlobalUbo.glsl"

// Input
in vec4 vPosition; // Position in screen space
in vec4 vTint;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vUV;

out vec4 fTint;
out vec3 fUV;
out mat3 fTBN;
out vec3 fNormal;

uniform vec3 unOffset;
uniform float unScale;
uniform mat4 unModelTransform;

void main() {
  fTint = vTint;
  fUV = vUV;
  vec4 scaledPos = vec4(vPosition.xyz * unScale, 1.0);
  vec4 transformedPos = unModelTransform * scaledPos;
  vec4 worldPos = transformedPos + vec4(unOffset, 0.0);
  gl_Position = VP * worldPos;
  
  vec3 normal = (unModelTransform * vec4(vNormal, 1.0)).rgb;
  vec3 tangent = (unModelTransform * vec4(vTangent, 1.0)).rgb;
  vec3 bitangent = (unModelTransform * vec4(vBitangent, 1.0)).rgb;
  fTBN = mat3(tangent, bitangent, normal);
}