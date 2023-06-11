uniform mat4 unVP;
uniform int unBufferStart;

in vec2 vPosition;

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

layout(std430, binding = 4) readonly buffer ParticlePosition
{
    vec2 ParticlePositions[];
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

vec4 getColor(uint bufferIndex) {
    uint packedColor = ParticleColors[bufferIndex];
    vec4 color;
    color.a = float((packedColor >> 24) & 0xFF);
    color.b = float((packedColor >> 16) & 0xFF);
    color.g = float((packedColor >> 8) & 0xFF);
    color.r = float(packedColor & 0xFF);
    return color / 255.0;
}


void main() {
    const int idx = indices[gl_VertexID % 6];
    const uint bufferIndex = (gl_VertexID / 6) + unBufferStart;
	vec2 offset = pos[idx];

    fUV = (offset.xy + 0.5);
    // Offset to particle position and scale
    vec2 position = offset.xy * ParticleScales[bufferIndex] + ParticlePositions[bufferIndex];
    fColor = getColor(bufferIndex);
    fParticleMaterial = ParticleMaterials[bufferIndex];
    gl_Position = unVP * vec4(position.xy, 0.0, 1.0);
}
