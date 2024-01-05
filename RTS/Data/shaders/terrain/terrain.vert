#include "GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;

uniform vec3 unPosition;
uniform vec2 unUVRoot;
uniform float unInverseWorldWidth;
uniform float unColorMapScale = 0.005;
uniform float unSnowLevel;

out float fHeight;
out vec3 fPosition;
out vec2 fUV;
out vec2 fBiomeUV;
out mat3 fTBN;
out float fSnow;
out vec3 fNormal;

const vec3 TANGENT = vec3(0.0, 1.0, 0.0);

void main() {
    vec4 vertexPos = vPosition;
    vec4 worldPos = vertexPos + vec4(unPosition - CameraPos, 0.0);
    fBiomeUV = (vertexPos.xy + unPosition.xy) * unInverseWorldWidth;
	
	vec3 normal = vNormal; // Prenormalized on CPU
	vec3 binormal = cross(normal, TANGENT);
    vec3 tangent = cross(binormal, normal);
    // TODO: TANGENT???
	fTBN = mat3(TANGENT, binormal, normal);
    fNormal = normal;
    
    fSnow = normal.z * unSnowLevel;
    fSnow += clamp((vertexPos.z - 50.0) * 0.025 * normal.z, 0.0, 3.0);
    worldPos.z += fSnow * 0.5f;
	
    fUV = unUVRoot + vPosition.xy * unColorMapScale;
    fHeight = vertexPos.z;
    fPosition = worldPos.xyz;

    gl_Position = VP * worldPos;
}