
const vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));

uniform uint unMaterialIndex;
uniform vec4 unTint;

out vec2 fUV;
flat out uint fMaterialIndex;
out vec4 fTint;
out mat3 fTBN;

void main() {

    gl_Position = vec4(vertices[gl_VertexID],0,1);
    
    fUV = 0.5 * gl_Position.xy + vec2(0.5);
    
    fMaterialIndex = unMaterialIndex;
    fTint = unTint;
    
    vec3 normal = vec3(0.0, 1.0, 0.0);
	vec3 tangent = vec3(1.0, 0.0, 0.0);
	vec3 binormal = cross(normal, tangent);
	fTBN = mat3(tangent, binormal, normal);
}
