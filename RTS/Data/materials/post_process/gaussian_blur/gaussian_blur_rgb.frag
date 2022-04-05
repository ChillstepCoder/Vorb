uniform sampler2D unInputFbo;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;
#include "../../GlobalUbo.glsl"


#include "../../util/mask_blur.glsl"

in vec2 fUV;

out vec4 fColor;

void main() {
    float baseAlpha = texture(unInputFbo, fUV).a;
	fColor.rgb = blur13rgbAlphaMask(unInputFbo, fUV, ScreenResolution, unDirection * baseAlpha);
	fColor.a = baseAlpha;

}