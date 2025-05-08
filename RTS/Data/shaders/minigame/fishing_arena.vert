uniform mat4 unVP;
uniform float unRadius;

in vec2 vPosition;

out vec2 fUV;


const vec2 pos[4] = vec2[4](
	vec2(-1.0, -1.0),
	vec2( 1.0, -1.0),
	vec2( 1.0, 1.0),
	vec2(-1.0, 1.0)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

void main() {
    int idx = indices[gl_VertexID];
	vec2 position = pos[idx];

    fUV = (position.xy + 1.0) * 0.5;
    // Offset to center
    gl_Position = unVP * vec4(position.xy * unRadius, 0.0, 1.0) + vec4(1.0, -1.0, 0.0, 0.0);
}
