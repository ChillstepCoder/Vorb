uniform sampler2D unInputFbo;
uniform sampler2D FboDepth;
uniform vec2 ScreenResolution;
uniform vec2 unDirection;

uniform vec2 unBlurRangeNear;
uniform vec2 unBlurRangeFar;
uniform float unBlurExponent;
uniform float unDebugRender;

#include "../../GlobalUbo.glsl"

in vec2 fUV;

out vec4 fColor;

#include "util/gaussian_blur.glsl"

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
}

void main() {
    float depth = texture2D(FboDepth, fUV).r;
	float isGround = 1.0 - step(0.999999999, depth);
    float linDepth = linearizeDepth(depth);
	// Blur far away
	float blurValue = smoothstep(unBlurRangeFar.x, unBlurRangeFar.y, linDepth) * isGround;
	// Blur near the camera
	blurValue = max(blurValue, (1.0 - smoothstep(unBlurRangeNear.x, unBlurRangeNear.y, linDepth)));
    blurValue = pow(blurValue, unBlurExponent);
	
    // Only blur non-sky
    if (isGround > 0.001) {
       fColor.rgb = blur13(unInputFbo, fUV, ScreenResolution, unDirection * blurValue).rgb;
    } else {
       fColor.rgb = texture2D(unInputFbo, fUV).rgb;
    }
	fColor.a = 1.0;
	
    // Debug rendering
	fColor.rgb = mix(fColor.rgb, vec3(blurValue), unDebugRender);
	
}