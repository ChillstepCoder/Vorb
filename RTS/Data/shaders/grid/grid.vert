
uniform mat4 unVP;
layout(location=0) out vec2 uv;

#include "GridParameters.h"


void main()
{

	int idx = indices[gl_VertexID];
	vec3 position = pos[idx] * gridSize;

	gl_Position = unVP * vec4(position, 1.0);
	uv = position.xy;
}
