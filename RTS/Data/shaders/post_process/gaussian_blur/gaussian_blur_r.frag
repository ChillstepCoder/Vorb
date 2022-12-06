uniform sampler2D unInputFbo;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;

in vec2 fUV;

out float fColor;
#include "../../util/gaussian_blur.glsl"

void main() {
	fColor = blur9r(unInputFbo, fUV, ScreenResolution, unDirection);
}