#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"

uniform mat4 unM;
uniform mat4 unVP;
uniform vec4 unPosOffset = vec4(0.0);
uniform vec3 unCameraPos;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in int vMaterialIndex;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 6) in float vWindInfluence;

out vec2 fUV;
out vec3 fWorldPos;
out vec2 fScreenPos;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fTangent;
out vec3 fViewTangent;
out vec3 fFragPosTangent;


void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    fMaterialIndex = vMaterialIndex;
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
    normal = (unM * vec4(normal.xyz, 0.0)).xyz;
    tangent = (unM * vec4(tangent.xyz, 0.0)).xyz;
    
	vec3 bitangent = cross(normal, tangent);
    tangent = cross(bitangent, normal);
    //tangent = normalize(cross(normal, bitangent));
    
    // For debugging
    fTangent = tangent;
    
	fTBN = mat3(tangent, bitangent, normal);
    
	
    
    vec4 worldPos = unM * (vPosition + unPosOffset);
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