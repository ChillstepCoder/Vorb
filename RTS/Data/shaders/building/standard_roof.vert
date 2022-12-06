#include "../GlobalUbo.glsl"

in vec4 vPosition;
in vec3 vNormal;
in vec2 vUV;
in vec4 vUVTiling;
in vec4 vTint;
in float vAtlasPage;

out vec2 fUV;
out vec4 fUVTiling;
out vec3 fNormal;
flat out float fAtlasPage;
out vec4 fTint;

void main() {
    fTint = vTint;
    fUV = vUV;
	fUVTiling = vUVTiling;
    fAtlasPage = vAtlasPage;
	fNormal = vNormal;
    vec4 worldPos = vPosition - vec4(CameraPos, 0.0);

    gl_Position = VP * worldPos;
}