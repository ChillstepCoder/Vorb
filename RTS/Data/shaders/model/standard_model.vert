#include "GlobalUbo.glsl"
#include "util/wind.glsl"
#include "util/uv.glsl"

uniform int unWindType = 0;
uniform float unSnowLevel;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;
//layout(location = 6) in float vWindInfluence;
layout(location = 7) in mat4 vModelMatrix;
layout(location = 8) in uint vMaterialIndex;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;
out float fSnow;

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
    
    fSnow = max(normal.z, 0.0) * unSnowLevel;
    
    vec4 adjustedPosition = vPosition;
    adjustedPosition.z += fSnow * 0.25f;
    
    vec4 trueWorldPos = (vModelMatrix * adjustedPosition);
    vec3 modelRoot = vModelMatrix[3].xyz;
    
    float height = adjustedPosition.z;
    
    float windPower = pow(1.0 - unSnowLevel, 4.0);
    addModelWind(trueWorldPos, modelRoot, unWindType, height * windPower);
    
    vec4 relativeWorldPos = trueWorldPos - vec4(CameraPos, 0.0);
    gl_Position = VP * relativeWorldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN); // Transpose is same as inverse for tbn because it is orthogonal, apparently
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * relativeWorldPos.xyz;
    
    // THIS IS FUNNY
   // fTint.rgb *= (sin(Time * fTint.g + height * 4.0 + trueWorldPos.x - trueWorldPos.y) + 1.0) * 0.5 + 0.5;
   // vec2 center = vec2(0.0, 0.0); // Assuming the center of the screen is (0,0) in NDC
   // vec2 toCenter = center - gl_Position.xy;
   // float distanceFromCenter = length(toCenter);
    //float angle = atan(toCenter.y, toCenter.x);
   // float spiralEffect = sin(angle + distanceFromCenter * 2.1); 
   // fTint.rgb += (vec3(spiralEffect) + vec3(1.0)) * 0.5;

}