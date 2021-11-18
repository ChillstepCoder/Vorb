uniform mat4 VP;
uniform float Time;
uniform vec3 CameraPos;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;
in vec3 vNormal;
in vec2 vTangent;
in float vWindInfluence;

out vec2 fUV;
flat out float fAtlasPage;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;

#include "../util/wind.glsl"

void main() {
    fTint = vTint;
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = vPosition - vec4(CameraPos, 0.0);
    worldPos.x += getWindAtPosition(Time, vPosition) * vWindInfluence;
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vec3(vTangent, 0));
	vec3 binormal = cross(normal, tangent);
	fTBN = mat3(tangent, binormal, normal);
	
	fRoughness = 0.2;

    gl_Position = VP * worldPos;
}