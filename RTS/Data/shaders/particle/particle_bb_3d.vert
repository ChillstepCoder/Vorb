#include "GlobalUbo.glsl"

uniform mat4 unVP;

uniform vec4 unGlobalColor = vec4(1.0);
uniform uint unGlobalMaterial = 0;
uniform vec2 unGlobalScale = vec2(1.0);
uniform uint unBaseInstanceOffset = 0;
uniform vec3 unRootPos = vec3(0.0);

// TODO: UBO
uniform uint unIsUsingColor = 0;
uniform uint unIsUsingHDRColor = 0;
uniform uint unIsUsingMaterial = 0;
uniform uint unIsUsingScale = 0;
uniform uint unIsUsingRotation = 0;

out vec2 fUV;
flat out uint fParticleMaterial;
flat out vec4 fColor;

const vec2 pos[4] = vec2[4](
	vec2(-0.5, -0.5),
	vec2(-0.5, 0.5),
	vec2( 0.5, 0.5),
	vec2( 0.5, -0.5)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

layout(std430, binding = 4) readonly buffer ParticlePosition
{
    vec3 ParticlePositions[];
};

layout(std430, binding = 5) readonly buffer ParticleScale
{
    vec2 ParticleScales[];
};

layout(std430, binding = 6) readonly buffer ParticleColor
{
    uint ParticleColors[];
};

layout(std430, binding = 7) readonly buffer ParticleHDRColor
{
    vec4 ParticleHDRColors[];
};

layout(std430, binding = 8) readonly buffer ParticleMaterial
{
    uint ParticleMaterials[];
};

layout(std430, binding = 11) readonly buffer ParticleXYOrient
{
    vec2 ParticleXYOrients[];
};

vec4 getColor(uint particleId) {
    uint packedColor = ParticleColors[particleId];
    vec4 color;
    color.a = float((packedColor >> 24) & 0xFF);
    color.b = float((packedColor >> 16) & 0xFF);
    color.g = float((packedColor >> 8) & 0xFF);
    color.r = float(packedColor & 0xFF);
    return (color / 255.0);
}

// Function to convert a normalized vec3 into Euler angles (yaw, pitch)
vec2 normalToEulerAngles(vec3 normal) {
    float yaw, pitch;

    // Yaw is the angle between the vector's projection on the XZ-plane and the X-axis
    yaw = atan(normal.z, normal.x);

    // Pitch is the angle between the vector and the XZ-plane
    pitch = atan(normal.y, length(normal.xz));

    return vec2(yaw, pitch);
}

mat3 getRotationMatrix(float yaw, float pitch, float roll) {
    float cy = cos(yaw);
    float sy = sin(yaw);
    float cp = cos(pitch);
    float sp = sin(pitch);
    float cr = cos(roll);
    float sr = sin(roll);
    
    // Construct the rotation matrix directly from Euler angles
    return mat3(
        cy * cr + sy * sp * sr,   -cy * sr + sy * sp * cr,   sy * cp,
        sr * cp,                   cr * cp,                  -sp,
       -sy * cr + cy * sp * sr,    sy * sr + cy * sp * cr,   cy * cp
    );
}

mat3 createTransformMatrix(float pitch, float roll) {
    // Calculate the cos and sin of the pitch, and roll
    float cp = cos(pitch);
    float sp = sin(pitch);
    float cr = cos(roll);
    float sr = sin(roll);

    // Create the rotation matrix for the pitch (around the Y-axis)
    mat3 pitchRotation = mat3(
        cp, 0, -sp,
        0, 1, 0,
        sp, 0, cp
    );

    // Create the rotation matrix for the roll (around the X-axis)
    mat3 rollRotation = mat3(
        1, 0, 0,
        0, cr, sr,
        0, -sr, cr
    );

    return pitchRotation * rollRotation;
}

void main() {
    const uint particleId = unBaseInstanceOffset + gl_VertexID / 6;

    const int idx = indices[gl_VertexID % 6];
	vec2 offset = pos[idx];

    fUV = (offset.xy + 0.5);
    fUV.y = 1.0 - fUV.y; // Flip
    vec3 position = ParticlePositions[particleId].xyz + (unRootPos - CameraPos);
    
    vec3 upOrient = CameraUp;
    vec3 rightOrient = CameraRight;
    
    // Color
    fColor = unGlobalColor;
    if (unIsUsingHDRColor == 1) {
        fColor *= ParticleHDRColors[particleId];
    } else if (unIsUsingColor == 1) {
        fColor *= getColor(particleId);
    }
    
    // Rotation
    if (unIsUsingRotation == 1) {
        // TODO: 3d
        vec2 rotationxy = ParticleXYOrients[particleId].xy;
        
        // NOTE: Each of these components is correct on their own but together they seem to break when
        // velocity is < 0 (for orient to velocity module)
       // mat3 rotation = createTransformMatrix(rotationxy.x, 0.0, 0.0);
        mat3 rotation = createTransformMatrix(rotationxy.y, rotationxy.x);
        
        // TODO: can this be optimized since we always multiply by 0,0,1?
        upOrient = rotation * vec3(0.0, 0.0, 1.0);
    }
    
    // Scale
    if (unIsUsingScale == 1) {
        vec2 scale = ParticleScales[particleId];
        position += offset.x * unGlobalScale.x * rightOrient * scale.x;
        position += offset.y * unGlobalScale.y * upOrient * scale.y;
    } else {
        position += offset.x * unGlobalScale.x * rightOrient;
        position += offset.y * unGlobalScale.y * upOrient;
    }
    
    // Material
    if (unIsUsingMaterial == 1) {
        fParticleMaterial = ParticleMaterials[particleId];
    } else {
        fParticleMaterial = unGlobalMaterial;
    }
    
    gl_Position = unVP * vec4(position.xyz, 1.0);
}
