uniform sampler2D FboShadowHeight;
uniform sampler2D FboDepth;
uniform float ZoomScale;
uniform float SunHeight;

in vec2 fUV;

out vec4 fColor;

const float DEPTH_SCALE = 10.0; // Far plane  is 10

// TODO: Shared constants file?
// Needs to match C++
const float SHADOW_SCALE = 6.0;

void main() {
	
	float depth = texture(FboDepth, fUV).r;
	float invertDepth = (1.0 - depth * 2.0) * DEPTH_SCALE;
	
	vec2 shadowUV = fUV;
	// Make objects feel 3D under shadows (Doesn't really work right)
	//shadowUV.y -= invertDepth * ZoomScale * 0.0002;
	float shadowHeight = texture(FboShadowHeight, shadowUV).r * SHADOW_SCALE;
	
	fColor.rgb = vec3(0.0);
	fColor.a = step(0.0, shadowHeight - (invertDepth + 0.01)) * SunHeight;
}