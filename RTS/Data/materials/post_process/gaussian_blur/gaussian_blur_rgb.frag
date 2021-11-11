uniform sampler2D unInputFbo;
uniform vec2 unPixelDims;
uniform vec2 unDirection;

in vec2 fUV;

out vec4 fColor;
#include "../../util/gaussian_blur.glsl"

void main() {
    float baseAlpha = texture(unInputFbo, fUV).a;
	//fColor.rgb = normalize(blur13noalpha(unInputFbo, fUV, unPixelDims, unDirection * baseAlpha));
	fColor.rgb = blur13noalpha(unInputFbo, fUV, unPixelDims, unDirection * baseAlpha);
	fColor.a = baseAlpha;
}