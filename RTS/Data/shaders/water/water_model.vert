#include "GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 2) in uint vMaterialIndex;
layout(location = 7) in mat4 vModelMatrix;

out vec3 fPosition;
out vec2 fUV;
out float fDepth;
out float fCameraDist;
flat out uint fMaterial;

const vec3 TANGENT = vec3(0.0, 1.0, 0.0);
const float DEPTH = 2.0f;

void main() {
    vec4 worldPos = vModelMatrix * vPosition;
	
    fMaterial = vMaterialIndex;
    fUV = worldPos.xy * 0.075;
    fDepth = DEPTH;
    float waveHeight = (cos(fUV.x - fUV.y + Time * 0.3) + 1.0) * 0.1;
    // Subtract so we always go below terrain, not above
    worldPos.z -= waveHeight;
    vec4 relativeWorldPos = worldPos - vec4(CameraPos, 0.0);
    
    fPosition = relativeWorldPos.xyz;
    fCameraDist = length(relativeWorldPos.rgb);

    gl_Position = VP * relativeWorldPos;
}