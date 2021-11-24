uniform sampler2D unInputFbo;
uniform sampler2D FboDepth;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;
#include "../../GlobalUbo.glsl"

in vec2 fUV;

out vec4 fColor;

#include "../../util/gaussian_blur.glsl"

//float linearizeDepth(float d) {
//    float zn = 2.0 * d - 1.0;
//    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
//}

void main() {
    float depth = texture2D(FboDepth, fUV).r;
	float isSky = 1.0 - step(0.999999999, depth);
	// Blur far away
	float blurValue = smoothstep(0.996, 1.00, depth) * isSky;
	// Blur near the camera
	blurValue = max(blurValue, (1.0 - smoothstep(0.0, 1.0, depth)) * 32.0);
	
    fColor.rgb = blur13noalpha(unInputFbo, fUV, ScreenResolution, unDirection * blurValue);
	fColor.a = 1.0;
	
	
	//fColor.rgb =  fColor.rgb * 0.00001 + blurValue;
	
}