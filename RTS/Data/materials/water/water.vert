#include "../GlobalUbo.glsl"

uniform vec3 unOffset;

in vec4 vPosition;
in float vDepth;

out vec3 fPosition;
out vec2 fUV;
out float fDepth;
out float fCameraDist;

const vec3 TANGENT = vec3(0.0, 1.0, 0.0);

void main() {
    vec4 vertexPos = vPosition;
    vec4 worldPos = vertexPos + vec4(unOffset, 0.0);
	
	
    fUV = (worldPos.xy + CameraPos.xy) * 0.075;
    fDepth = vDepth;
    float waveHeight = (cos(fUV.x - fUV.y + Time * 0.3) + 1.0) * 0.1;
    // Subtract so we always go below terrain, not above
    worldPos.z -= waveHeight;
    fPosition = worldPos.xyz;
    fCameraDist = length(worldPos.rgb);

    gl_Position = VP * worldPos;
}