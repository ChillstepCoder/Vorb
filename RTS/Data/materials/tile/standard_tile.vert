uniform mat4 VP;
uniform vec3 CameraPos;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;

out vec2 fUV;
flat out float fAtlasPage;
out vec4 fTint;

void main() {
    fTint = vTint;
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = vPosition - vec4(CameraPos, 0.0);
    gl_Position = VP * worldPos;
}