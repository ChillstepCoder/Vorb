
const vec3 pos[4] = vec3[4](
	vec3(-1.0, -1.0, 0.0),
	vec3( 1.0, -1.0, 0.0),
	vec3( 1.0, 1.0, 0.0),
	vec3(-1.0, 1.0, 0.0)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

uniform uint unMaterialIndex;
uniform vec4 unTint;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;

void main() {
	int idx = indices[gl_VertexID];
	vec3 position = pos[idx];

	gl_Position = vec4(position, 1.0);
    
	fUV = position.xy;
    fMaterialIndex = unMaterialIndex;
    fTint = unTint;
    
    vec3 normal = vec3(0.0, 1.0, 0.0);
	vec3 tangent = vec3(1.0, 0.0, 0.0);
	vec3 binormal = cross(normal, tangent);
	fTBN = mat3(tangent, binormal, normal);
}
