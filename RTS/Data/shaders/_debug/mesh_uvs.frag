
#include "TextureUbo.glsl"

in vec2 fUV;

layout (location=0) out vec4 oColor;


void main()
{
  
	oColor.rg = fUV;
    oColor.b = 0.0;
    oColor.a = 1.0;

};
