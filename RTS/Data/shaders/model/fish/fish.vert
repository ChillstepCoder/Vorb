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
uniform float DebugFloat1;
uniform float DebugFloat4;

struct FishData {
    vec4 posTurn;
    vec4 yawPitchScaleTime;
};

layout (std430, binding=3) buffer FishDataBuffer
{
    FishData fishDataArray[];
};

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;
out vec3 fViewTangent;
out vec3 fFragPosTangent;


mat4 createTransformMatrix(vec3 position, float yaw, float pitch, float scale) {
    // Calculate the cos and sin of the yaw and pitch
    float cy = cos(yaw);
    float sy = sin(yaw);
    float cp = cos(pitch);
    float sp = sin(pitch);

    // Create the rotation matrix for the yaw (around the Z-axis)
    mat3 yawRotation = mat3(
        cy, sy, 0,
        -sy, cy, 0,
        0, 0, 1
    );

    // Create the rotation matrix for the pitch (around the Y-axis)
    mat3 pitchRotation = mat3(
        cp, 0, -sp,
        0, 1, 0,
        sp, 0, cp
    );

    // Combine the two rotations and scale the result
    mat3 rotation = yawRotation * pitchRotation;
    rotation *= scale;

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

vec2 rotateVector(vec2 xy, float angle) {
    const float cs = cos(angle);
    const float sn = sin(angle);

    vec2 rv;
    rv.x = xy.x * cs - xy.y * sn;
    rv.y = xy.x * sn + xy.y * cs;
    return rv;
}

void main() {
    fTint = vTint;
    fUV = unpackUV(vUV);
    fMaterialIndex = vMaterialIndex;
    
    vec3 vertexPosition = vPosition.xyz;
    
    FishData fishData = fishDataArray[gl_InstanceID + unBufferOffset];
    
    vec3 rootPosWorld = fishData.posTurn.xyz;
    
    // Movement wiggle
    float intensityMult = 0.1 + DebugFloat4 * 0.1;
    float timeValue = (Time - (rootPosWorld.x - rootPosWorld.y + rootPosWorld.z));
    vertexPosition.y += cos(-timeValue + -vertexPosition.x * 3.0) * (-vertexPosition.x + 0.5) * intensityMult;
    rootPosWorld.z += sin(timeValue) * 0.05;
    
    // Turning rotation
    float turning = fishData.posTurn.w;
    vertexPosition.xy = rotateVector(vertexPosition.xy, turning * vPosition.x);
    
    
    mat4 modelMatrix = createTransformMatrix(rootPosWorld, fishData.yawPitchScaleTime.x, -fishData.yawPitchScaleTime.y, fishData.yawPitchScaleTime.z);
    //mat4 modelMatrix = buildTranslation(transform.posYaw.xyz);
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vTangent);
    
    mat3 modelMatrix3 = mat3(modelMatrix);
    normal = modelMatrix3 * normal;
    tangent = modelMatrix3 * tangent;
    
	vec3 bitangent = cross(normal, tangent);
	fTBN = mat3(tangent, bitangent, normal);
    
    
    vec4 worldPos = (modelMatrix * vec4(vertexPosition, 1.0)) - vec4(CameraPos, 0.0);
    gl_Position = VP * worldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN); // Transpose is same as inverse for tbn because it is orthogonal, apparently
    fViewTangent  = vec3(0.0); // tfTBN * CameraPos; // TODO: Is this right?
    fFragPosTangent  = tfTBN * worldPos.xyz;
}