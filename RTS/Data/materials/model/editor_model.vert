#include "../TextureUbo.glsl"
#include "../GlobalUbo.glsl"
#include "../util/wind.glsl"

uniform vec3 unCameraPos;
uniform mat4 unVP;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in int vTextureIndex;
layout(location = 3) in vec4 vTint;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec2 vTangent;
//layout(location = 6) in float vWindInfluence;

out vec2 fUV;
flat out int fTextureIndex;
out vec4 fTint;
out mat3 fTBN;
out float fRoughness;




void main() {
    fTint = vTint;
    fUV = vUV;
    fTextureIndex = vTextureIndex;
	
	vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vec3(vTangent, 0));
	vec3 binormal = cross(normal, tangent);
	fTBN = mat3(tangent, binormal, normal);
	
	fRoughness = 0.2;
    // TODO: unPosition?
    vec4 worldPos = vPosition + vec4(-unCameraPos, 0.0);
    //worldPos.x += getWindAtPosition(Time, vPosition) * 1.0;
    gl_Position = unVP * worldPos;
}