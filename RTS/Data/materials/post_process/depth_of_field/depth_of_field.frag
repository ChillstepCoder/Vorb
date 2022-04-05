uniform sampler2D unInputFbo;
uniform sampler2D FboDepth;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;
#include "../../GlobalUbo.glsl"

in vec2 fUV;

out vec4 fColor;

#include "../../util/mask_blur.glsl"

//float linearizeDepth(float d) {
//    float zn = 2.0 * d - 1.0;
//    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
//}

void main() {
    float depth = texture2D(FboDepth, fUV).r;
	float isGround = 1.0 - step(0.999999999, depth);
	// Blur far away
	float blurValue = smoothstep(0.996, 1.00, depth) * isGround;
	// Blur near the camera
	blurValue = max(blurValue, (1.0 - smoothstep(0.0, 1.0, depth)) * 8.0);
	
    // Only blur non-sky
    if (isGround > 0.001) {
       fColor.rgb = blur13rgbDepthMask(unInputFbo, FboDepth, depth, fUV, ScreenResolution, unDirection * blurValue);
    } else {
       fColor.rgb = texture2D(unInputFbo, fUV).rgb;
    }
	fColor.a = 1.0;
	
	//fColor.rgb =  fColor.rgb * 0.00001 + blurValue;
	
}