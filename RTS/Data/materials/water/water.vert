#include "../GlobalUbo.glsl"

uniform vec3 unOffset;

in vec4 vPosition;
in float vDepth;

out vec3 fPosition;
out vec2 fUV;
out float fDepth;
out float fWaveHeight;

const vec3 TANGENT = vec3(0.0, 1.0, 0.0);

void main() {
    vec4 vertexPos = vPosition;
    vec4 worldPos = vertexPos + vec4(unOffset, 0.0);
	
	
    fUV = (worldPos.xy + CameraPos.xy) * 0.05;
    fDepth = vDepth;
    fWaveHeight = cos(fUV.x - fUV.y + Time * 0.3) * 0.2;
    worldPos.z += fWaveHeight;
    fPosition = worldPos.xyz;

    gl_Position = VP * worldPos;
}