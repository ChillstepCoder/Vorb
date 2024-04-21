
layout(location=0) in vec2 uv;
layout(location=0) out vec4 oColor;
layout(location=1) out vec3 oNormal;


#include "GridParameters.h"
#include "GridCalculation.h"


void main()
{
	oColor = gridColor(uv);
    
    float coneThinness = 8.0; // 1.0 is max thickness, larger is thinner 
    oColor.r += step(-uv.x + abs(uv.y * coneThinness), 0.0);
    oColor.g += step(-uv.y + abs(uv.x * coneThinness), 0.0);
    oNormal = vec3(0.0);
};
