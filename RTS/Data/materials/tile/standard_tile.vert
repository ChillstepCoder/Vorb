uniform mat4 World;
uniform mat4 VP;

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
    vec4 worldPos = World * vPosition;
    gl_Position = VP * worldPos;
	//gl_Position.z += (gl_Position.y + 1.0) / 2.0;
}