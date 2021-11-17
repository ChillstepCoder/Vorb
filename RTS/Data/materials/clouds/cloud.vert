uniform mat4 VP;
uniform float Time;
uniform vec3 CameraRight;
uniform vec3 CameraFront;
uniform vec3 CameraUp;
uniform vec3 CameraPos;

in vec4 vPosition;
in vec2 vXZOffset;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;

out vec2 fUV;
out vec2 fPosition;
flat out float fAtlasPage;
out vec4 fTint;


void main() {
    fTint = vTint;
    fUV = vUV;
    fAtlasPage = vAtlasPage;
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vXZOffset / 100.0; // Matches C++ compression ratio
	vertexPosition.xyz += CameraUp * xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	// Hacky way to make the x,z offsets all 1
	fPosition = clamp(xzOffsetUncompressed * 10.0, -1.0, 1.0);
	fPosition = (fPosition + 1.0) * 0.5; // 0 - 1 range
	
	vec4 worldPos = vertexPosition - vec4(CameraPos, 0.0);

	//fTint.r = 1.0 - angle;
	//fTint.g = 0.0;
	//fTint.b = 0.0;
	
	vec4 glPos = VP * worldPos;
    gl_Position = glPos;
}