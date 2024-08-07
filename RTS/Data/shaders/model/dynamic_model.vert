#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"
#include "model/model_variant.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialSlot;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 6) in float vWindInfluence;
layout(location = 7) in mat4 vModelMatrix;
layout(location = 11) in uint vVariantIndex;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    fMaterialIndex = inVariantData[vVariantIndex].materials[vMaterialSlot];
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
    mat3 modelMatrix3 = mat3(vModelMatrix);
    normal = modelMatrix3 * normal;
    tangent = modelMatrix3 * tangent;
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
    
    vec4 relativeWorldPos = (vModelMatrix * vPosition);
    vec3 modelRoot = vModelMatrix[3].xyz;
    
    float height = vPosition.z;
    
    gl_Position = VP * relativeWorldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN); // Transpose is same as inverse for tbn because it is orthogonal, apparently
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * relativeWorldPos.xyz;

}