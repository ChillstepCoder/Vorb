#include "../TextureUbo.glsl"
#include "../GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;

out float fHeight;
out vec3 fPosition;
out vec2 fUV;
out mat3 fTBN;

const vec3 TANGENT = vec3(0.0, 1.0, 0.0);

void main() {
    vec4 vertexPos = vPosition;
    vec4 worldPos = vertexPos + vec4(unPosition - CameraPos, 0.0);
	
	vec3 normal = vNormal; // Prenormalized on CPU
	vec3 binormal = cross(normal, TANGENT);
    vec3 tangent = cross(binormal, normal);
    // TODO: TANGENT???
	fTBN = mat3(TANGENT, binormal, normal);
	
    fUV = (worldPos.xy + CameraPos.xy) * 0.05;
    fHeight = vertexPos.z;
    fPosition = worldPos.xyz;

    gl_Position = VP * worldPos;
}