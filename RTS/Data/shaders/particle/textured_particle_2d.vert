uniform mat4 unVP;

uniform vec4 unGlobalColor = vec4(1.0);
uniform uint unGlobalMaterial = 0;
uniform vec2 unGlobalScale = vec2(1.0);

uniform uint unIsUsingColor = 0;
uniform uint unIsUsingMaterial = 0;
uniform uint unIsUsingScale = 0;

out vec2 fUV;
flat out uint fParticleMaterial;
flat out vec4 fColor;

const vec2 pos[4] = vec2[4](
	vec2(-0.5, -0.5),
	vec2( 0.5, -0.5),
	vec2( 0.5, 0.5),
	vec2(-0.5, 0.5)
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

layout(std430, binding = 7) readonly buffer ParticleMaterial
{
    uint ParticleMaterials[];
};

vec4 getColor() {
    uint packedColor = ParticleColors[gl_InstanceID];
    vec4 color;
    color.a = float((packedColor >> 24) & 0xFF);
    color.b = float((packedColor >> 16) & 0xFF);
    color.g = float((packedColor >> 8) & 0xFF);
    color.r = float(packedColor & 0xFF);
    return (color / 255.0);
}


void main() {
    const int idx = indices[gl_VertexID % 6];
	vec2 offset = pos[idx];

    fUV = (offset.xy + 0.5);
    
    vec2 position = offset.xy * unGlobalScale;
    
    // Scale
    if (unIsUsingScale == 1) {
        position *= ParticleScales[gl_InstanceID];
    }
    
    // Translation
    position += ParticlePositionsAndRotations[gl_InstanceID].xy;
    
    // Color
    fColor = unGlobalColor;
    if (unIsUsingColor == 1) {
        fColor *= getColor();
    }
    
    // Material
    if (unIsUsingMaterial == 1) {
        fParticleMaterial = ParticleMaterials[gl_InstanceID];
    } else {
        fParticleMaterial = unGlobalMaterial;
    }
    gl_Position = unVP * vec4(position.xy, 0.0, 1.0);
}
