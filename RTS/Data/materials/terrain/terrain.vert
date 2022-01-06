#include "../GlobalUbo.glsl"

uniform vec3 unOffset;
uniform float DebugFloat1;

in vec4 vPosition;
in vec3 vNormal;
in vec2 vTangent;

out float fHeight;
out vec3 fPosition;
out vec2 fUV;
out mat3 fTBN;
out float fRoughness;

const vec3 TANGENT = vec3(0.0, 1.0, 0.0);

void main() {
    vec4 vertexPos = vPosition;
    vertexPos.z *= (1.0 + DebugFloat1 * 5.0);
    vec4 worldPos = vertexPos + vec4(unOffset, 0.0);
	
	vec3 normal = vNormal; // Prenormalized on CPU
	vec3 binormal = cross(normal, TANGENT);
    vec3 tangent = cross(binormal, normal);
	fTBN = mat3(TANGENT, binormal, normal);
	
	fRoughness = 0.85;
    fUV = vertexPos.xy * 0.05;
    fHeight = vertexPos.z;
    fPosition = worldPos.xyz;

    gl_Position = VP * worldPos;
}