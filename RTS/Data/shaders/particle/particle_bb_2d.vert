uniform mat4 unVP;

uniform vec4 unGlobalOverlayColor = vec4(0.0);
uniform vec4 unGlobalColor = vec4(1.0);
uniform uint unGlobalMaterial = 0;
uniform vec2 unGlobalScale = vec2(1.0);
uniform uint unBaseInstanceOffset = 0;
uniform vec3 unRootPos = vec3(0.0);

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

// Includes a padding float that we can use later
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

vec2 rotateVector(vec2 pos, float angleRad) {
    const float cs = cos(angleRad);
    const float sn = sin(angleRad);

    vec2 rv;
    rv.x = pos.x * cs - pos.y * sn;
    rv.y = pos.x * sn + pos.y * cs;
    return rv;
}


void main() {
    const uint particleId = unBaseInstanceOffset + gl_VertexID / 6;

    const int idx = indices[gl_VertexID % 6];
	vec2 offset = pos[idx];

    fUV = (offset.xy + 0.5);
    fUV.y = 1.0 - fUV.y; // Flip
    
    vec2 position = offset.xy * unGlobalScale + unRootPos.xy;
    
    // Rotation
    if (unIsUsingRotation == 1) {
        position = rotateVector(position, ParticleXYOrients[particleId].x);
    }
    
    // Scale
    if (unIsUsingScale == 1) {
        position *= ParticleScales[particleId];
    }
    
    // Translation
    position += ParticlePositions[particleId].xy;
    
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
    gl_Position = unVP * vec4(position.xy, 0.0, 1.0);
}
