
layout(location=0) in vec2 uv;
layout(location=0) out vec4 oColor;
layout(location=1) out vec3 oNormal;


#include "GridParameters.h"
#include "GridCalculation.h"


void main()
{
	oColor = gridColor(uv);
    oNormal = vec3(0.0);
};
