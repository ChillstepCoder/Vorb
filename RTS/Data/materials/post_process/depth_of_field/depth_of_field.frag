uniform sampler2D unInputFbo;
uniform sampler2D FboDepth;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;

in vec2 fUV;

out vec4 fColor;

#include "../../util/gaussian_blur.glsl"

void main() {
    float depth = texture2D(FboDepth, fUV).r;
	float blurIntensity = 1.0 - depth;
    fColor.rgb = blur13noalpha(unInputFbo, fUV, ScreenResolution, unDirection * blurIntensity);
	fColor.a = 1.0;
	//fColor.rgb =  fColor.rgb * 0.00001 + blurIntensity;
}