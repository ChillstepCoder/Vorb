#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"


layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialIndex;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 6) in float vWindInfluence;

uniform vec3 unPosition;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    fMaterialIndex = vMaterialIndex;
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
    
    vec4 worldPos = vPosition + vec4(unPosition - CameraPos, 0.0);
    //worldPos.x += getWindAtPosition(Time, vPosition) * vWindInfluence;
    gl_Position = VP * worldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = inverse(fTBN);
    fViewTangent  = tfTBN * CameraPos;
    fFragPosTangent  = tfTBN * worldPos.xyz;
}