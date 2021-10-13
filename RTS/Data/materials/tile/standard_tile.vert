uniform mat4 VP;
uniform float Time;
uniform vec3 CameraPos;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;
in float vWindInfluence;

out vec2 fUV;
flat out float fAtlasPage;
out vec4 fTint;

#include "wind.glsl"

void main() {
    fTint = vTint;
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = vPosition - vec4(CameraPos, 0.0);
    worldPos.x += getWindAtPosition(Time, vPosition) * vWindInfluence;

    gl_Position = VP * worldPos;
}