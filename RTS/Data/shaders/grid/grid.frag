
layout(location=0) in vec2 uv;
layout(location=0) out vec4 out_FragColor;


#include "GridParameters.h"
#include "GridCalculation.h"


void main()
{
	out_FragColor = gridColor(uv);
};
