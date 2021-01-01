uniform sampler2D FboShadowHeight;
uniform sampler2D FboDepth;
uniform float ZoomScale;
uniform float SunHeight;
uniform float SunPosition;

in vec2 fUV;

out vec4 fColor;

const float DEPTH_SCALE = 256.0; // Far plane is 256

// TODO: Shared constants file?
// Needs to match C++ HEIGHT_SCALE
const float SHADOW_SCALE = 62.0;
const float SHADOW_MAX_ALPHA = 0.5;

void main() {
	
	float depth = texture(FboDepth, fUV).r;
	float invertDepth = (1.0 - depth * 2.0) * DEPTH_SCALE;
	
	vec2 shadowUV = fUV;
	// Make objects feel 3D under shadows (Doesn't really work right)
	//shadowUV.y -= invertDepth * ZoomScale * 0.0002;
	float shadowHeight = texture(FboShadowHeight, shadowUV).r;
	float shadowMult = shadowHeight * SHADOW_SCALE;
	
	fColor.rgb = vec3(0.0);
	// Becomes lighter near noon
	float shadowAlpha = pow(abs(SunPosition * SHADOW_MAX_ALPHA), 0.6);
	// Compile intensity
	fColor.a = step(0.0, shadowMult - (invertDepth + 0.01)) * SunHeight * shadowAlpha;
	// Lighter the higher it goes (doesn't work due to depth sort)
}