
#include "../GlobalUbo.glsl"

uniform mat4 unVP;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;

out vec2 fUV;

void main()
{
	gl_Position = unVP * vPosition;
    fUV = vUV;
}
