#include "../TextureUbo.glsl"
#include "../GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in int vTextureIndex;
//layout(location = 3) in vec4 vTint;
//layout(location = 4) in vec3 vNormal;
//layout(location = 5) in vec2 vTangent;
//layout(location = 6) in float vWindInfluence;
layout(location = 7) in vec3 iPosition;

out vec2 fUV;
flat out int fTextureIndex;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;


void main() {
    fTint = vec4(1.0);
    fUV = vUV;
    fTextureIndex = vTextureIndex;
    //worldPos.x += getWindAtPosition(Time, vPosition) * vWindInfluence;
	
	//vec3 normal = normalize(vNormal);
	//vec3 tangent = normalize(vec3(vTangent, 0));
	//vec3 binormal = cross(normal, tangent);
	fTBN = mat3(vec3(0.0), vec3(0.0), vec3(0.0));
	
	fRoughness = 0.2;

    vec4 worldPos = vPosition * 2.0 + vec4(iPosition - CameraPos, 0.0);
    gl_Position = VP * worldPos;
}