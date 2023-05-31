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

uniform int unBufferOffset;

struct FishTransformData {
    vec4 posYaw;
};

layout (std430, binding=3) buffer TransformData
{
    FishTransformData transformData[];
};

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;

mat4 createTransformMatrix(vec3 position, float yaw, float pitch) {
    // Calculate the cos and sin of the yaw and pitch
    float cy = cos(yaw);
    float sy = sin(yaw);
    float cp = cos(pitch);
    float sp = sin(pitch);
    
    // Manually create the rotation matrix for yaw and pitch
    mat3 rotation = mat3(cy*cp, sy, cy*sp,
                        -sy*cp, cy, -sy*sp,
                        -sp, 0, cp);
    
    // Create transformation matrix from rotation and translation
    mat4 transformation = mat4(rotation);
    transformation[3] = vec4(position, 1.0);
    
    return transformation;
}

mat4 buildTranslation(vec3 delta)
{
    return mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(delta, 1.0));
}

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    fMaterialIndex = vMaterialIndex;
    
    FishTransformData transform = transformData[gl_InstanceID + unBufferOffset];
    mat4 modelMatrix = createTransformMatrix(transform.posYaw.xyz, transform.posYaw.w, 1.57079632679);
    //mat4 modelMatrix = buildTranslation(transform.posYaw.xyz);
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
    mat3 modelMatrix3 = mat3(modelMatrix);
    normal = modelMatrix3 * normal;
    tangent = modelMatrix3 * tangent;
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
    
    vec4 worldPos = (modelMatrix * vPosition) - vec4(CameraPos, 0.0);
    gl_Position = VP * worldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN); // Transpose is same as inverse for tbn because it is orthogonal, apparently
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * worldPos.xyz;
}