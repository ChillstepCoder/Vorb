uniform mat4 unVP;
uniform int unBufferStart;

in vec2 vPosition;

out vec2 fUV;
flat out uint fBufferIndex;

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

void main() {
    const int idx = indices[gl_VertexID % 6];
    fBufferIndex = (gl_VertexID / 6) + unBufferStart;
	vec2 offset = pos[idx];

    fUV = (offset.xy + 0.5);
    // Offset to particle position and scale
    vec2 position = offset.xy * ParticleScales[fBufferIndex] + ParticlePositions[fBufferIndex];
    gl_Position = unVP * vec4(position.xy, 0.0, 1.0);
}
