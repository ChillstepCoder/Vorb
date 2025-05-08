#include "GlobalUbo.glsl"
#include "util/uv.glsl"
#include "util/tbn.glsl"
#include "model/model_variant.glsl"

uniform float unSnowLevel;
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in uint vMaterialSlot;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec3 vTangent;

uniform mat4 unVP;
uniform mat4 unModelMatrix = mat4(1.0);
uniform uint unVariantIndex = 0;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;
out vec3 fLocalPosition;

float damageTest(inout vec4 position, float damageValue) {
    vec2 offset = position.xy;
    float len = length(offset);
    vec2 offsetNormalized = offset / len;
    len = max(len * (1.0 - damageValue), min(len * 0.3, 1.0));
    position.xy = offsetNormalized * len;
    return damageValue;
}

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    fMaterialIndex = inVariantMaterials[unVariantIndex + vMaterialSlot];

	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
	fTBN = computeTbn(mat3(unModelMatrix), normal, tangent);
    
    vec4 adjustedPosition = vPosition;
    
    vec4 trueWorldPos = (unModelMatrix * vPosition);
    
    gl_Position = unVP * trueWorldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN); // Transpose is same as inverse for tbn because it is orthogonal, apparently
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * trueWorldPos.xyz;
}