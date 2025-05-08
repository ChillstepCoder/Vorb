#include "GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 4) in vec3 vNormal;

uniform mat4 unModelMatrix;
uniform float unSize;

out vec3 fNormal;
out vec3 fPosition;

void main() {
	
	vec3 normal = normalize(vNormal);
    
    vec4 position = vPosition;
    position.xyz += normal * unSize;
    
    mat3 modelMatrix3 = mat3(unModelMatrix);
    fNormal = modelMatrix3 * normal;
    
    vec4 trueWorldPos = (unModelMatrix * position);
    vec4 relativeWorldPos = trueWorldPos - vec4(CameraPos, 0.0);
    fPosition = relativeWorldPos.xyz;
    gl_Position = VP * relativeWorldPos;
    
}