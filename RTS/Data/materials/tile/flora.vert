uniform mat4 World;
uniform mat4 VP;
uniform float Time;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;

out vec2 fUV;
out vec2 fUVZCutout;
flat out float fAtlasPage;
out vec4 fTint;
out vec2 fScreenPos;
out vec3 fWorldPos;

#include "wind.glsl"

void main() {
    fTint = vTint;
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = World * vPosition;
    
    // Wind
    worldPos.x += getWindAtPosition(Time, worldPos);
    
	fWorldPos = worldPos.xyz;
    gl_Position = VP * worldPos;
	fScreenPos.xy = gl_Position.xy;
	fUVZCutout = (gl_Position.xy + 1.0) * 0.5;
}