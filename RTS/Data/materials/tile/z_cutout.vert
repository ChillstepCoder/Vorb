uniform mat4 World;
uniform mat4 VP;

in vec4 vPosition;
in vec2 vUV;
in float vAtlasPage;

out vec2 fUV;
flat out float fAtlasPage;
out vec3 fPosition;

void main() {
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = World * vPosition;
    gl_Position = VP * worldPos;
	fPosition.xy = gl_Position.xy;
	fPosition.z = vPosition.z; // Use world Z and screen XY?
}