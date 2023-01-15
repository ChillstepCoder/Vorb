
#include "../GlobalUbo.glsl"

uniform mat4 unVP;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in int vTextureIndex;
layout(location = 4) in vec3 vNormal;
layout(location = 5) in vec2 vTangent;

out vec2 fUV;
flat out int fTextureIndex;
out mat3 fTBN;

void main()
{
	gl_Position = unVP * vPosition;
    fUV = vUV;
    fTextureIndex = vTextureIndex;

    vec3 normal = normalize(vNormal);
	vec3 tangent = normalize(vec3(vTangent, 0));
	vec3 binormal = cross(normal, tangent);
	fTBN = mat3(tangent, binormal, normal);

}
