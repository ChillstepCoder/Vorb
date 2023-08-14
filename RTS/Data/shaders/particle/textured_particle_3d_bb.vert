#include "GlobalUbo.glsl"

uniform mat4 unVP;

uniform vec4 unGlobalOverlayColor = vec4(0.0);
uniform vec4 unGlobalColor = vec4(1.0);
uniform uint unGlobalMaterial = 0;
uniform vec2 unGlobalScale = vec2(1.0);
uniform uint unBaseInstanceOffset = 0;
uniform vec3 unRootOffset = vec3(0.0);

uniform uint unIsUsingColor = 0;
uniform uint unIsUsingHDRColor = 0;
uniform uint unIsUsingMaterial = 0;
uniform uint unIsUsingScale = 0;

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

layout(std430, binding = 4) readonly buffer ParticlePositionAndRotation
{
    vec4 ParticlePositionsAndRotations[]; // w is rotation
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

vec4 getColor(uint particleId) {
    uint packedColor = ParticleColors[particleId];
    vec4 color;
    color.a = float((packedColor >> 24) & 0xFF);
    color.b = float((packedColor >> 16) & 0xFF);
    color.g = float((packedColor >> 8) & 0xFF);
    color.r = float(packedColor & 0xFF);
    return (color / 255.0);
}


void main() {
    const uint particleId = unBaseInstanceOffset + gl_VertexID / 6;

    const int idx = indices[gl_VertexID % 6];
	vec2 offset = pos[idx];

    fUV = (offset.xy + 0.5);
    fUV.y = 1.0 - fUV.y; // Flip
    
    vec3 position = ParticlePositionsAndRotations[particleId].xyz + unRootOffset;
    
    // Scale
    if (unIsUsingScale == 1) {
        vec2 scale = ParticleScales[particleId];
        position += offset.x * unGlobalScale.x * CameraRight * scale.x;
        position += offset.y * unGlobalScale.y * CameraUp * scale.y;
    } else {
        position += offset.x * unGlobalScale.x * CameraRight;
        position += offset.y * unGlobalScale.y * CameraUp;
    }
    
    // Color
    fColor = unGlobalColor;
    if (unIsUsingHDRColor == 1) {
        fColor *= ParticleHDRColors[particleId];
    } else if (unIsUsingColor == 1) {
        fColor *= getColor(particleId);
    }
    
    // Material
    if (unIsUsingMaterial == 1) {
        fParticleMaterial = ParticleMaterials[particleId];
    } else {
        fParticleMaterial = unGlobalMaterial;
    }
    gl_Position = unVP * vec4(position.xyz, 1.0);
}
