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
    //worldPos.x += getWindAtPosition(Time, vPosition) * vWindInfluence;
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vec3(vTangent, 0));
	vec3 binormal = cross(normal, tangent);
	fTBN = mat3(tangent, binormal, normal);
	
	fRoughness = 0.2;
    
    //mat4 MVP = VP * iModels[gl_InstanceID];
    
    vec4 worldPos = (vModelMatrix * vPosition) - vec4(CameraPos, 0.0);
    //worldPos.x += getWindAtPosition(Time, vPosition) * 1.0;
    gl_Position = VP * worldPos;
}