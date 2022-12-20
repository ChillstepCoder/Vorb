#include "../TextureUbo.glsl"
#include "../GlobalUbo.glsl"
#include "../util/wind.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialIndex;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec2 vTangent;
//layout(location = 6) in float vWindInfluence;
layout(location = 7) in mat4 vModelMatrix;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;

void main() {
    fTint = vTint;
    fUV = vUV;
    fMaterialIndex = vMaterialIndex;
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vec3(vTangent, 0));
    normal = (vModelMatrix * vec4(normal, 0.0)).rgb;
    tangent = (vModelMatrix * vec4(tangent, 0.0)).rgb;
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
	
	fRoughness = 0.2;
    
    vec4 worldPos = (vModelMatrix * vPosition) - vec4(CameraPos, 0.0);
    gl_Position = VP * worldPos;
}