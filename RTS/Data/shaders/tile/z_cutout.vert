uniform mat4 VP;
uniform vec3 CameraPos;

in vec4 vPosition;
in vec2 vUV;
in float vAtlasPage;

out vec2 fUV;
flat out float fAtlasPage;
out vec3 fPosition;

void main() {
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = vPosition - vec4(CameraPos, 0.0);
    gl_Position = VP * worldPos;
	fPosition.xy = gl_Position.xy;
	fPosition.z = worldPos.z; // Use world Z and screen XY?
}