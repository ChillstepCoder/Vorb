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
layout(location = 7) in mat4 vModelMatrix;

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
    
    mat3 modelMatrix3 = mat3(vModelMatrix);
    normal = modelMatrix3 * normal;
    tangent = modelMatrix3 * tangent;
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
    
    vec4 worldPos = (vModelMatrix * vPosition) - vec4(CameraPos, 0.0);
    gl_Position = VP * worldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = inverse(fTBN);
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * worldPos.xyz;
}