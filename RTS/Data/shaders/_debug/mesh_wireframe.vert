
#include "../GlobalUbo.glsl"

uniform mat4 unVP;
uniform mat4 unM;

uniform vec4 unPosOffset = vec4(0.0);

layout(location = 0) in vec4 vPosition;

layout (location=0) out vec2 uv;
layout (location=1) out vec3 wpos;

void main()
{
	gl_Position = unVP * unM * (vPosition + unPosOffset);

	wpos = vPosition.xyz;
	uv = vec2(0.5, 0.5);
}
