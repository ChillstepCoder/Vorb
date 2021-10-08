uniform mat4 VP;
uniform float Time;
uniform vec3 CameraRight;
uniform vec3 CameraFront;
uniform vec3 CameraPos;

in vec4 vPosition;
in vec2 vXZOffset;
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
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vXZOffset / 100.0; // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	
	vec4 worldPos = vertexPosition - vec4(CameraPos, 0.0);
    
    // Wind
    worldPos.x += getWindAtPosition(Time, vertexPosition) * vWindInfluence;
	
	vec4 glPos = VP * worldPos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	
	// Lean away at top
	vec3 glPosNoX = vec3(0.0, min(glPos.y, -1.0), glPos.z);
	float angle = 1.0 - dot(screenCamera.xyz, normalize(glPosNoX));
	
	angle = min(pow(angle, 0.4) * xzOffsetUncompressed.y, 1.0);
	worldPos.xyz += CameraFront * angle;
	glPos = VP * worldPos;
	
	//fTint.r = 1.0 - angle;
	//fTint.g = 0.0;
	//fTint.b = 0.0;
	
	
    gl_Position = glPos;
}