uniform sampler2D Fbo0;
uniform sampler2D unShadowFbo;
uniform vec3 ShadowColor;

#include "../../GlobalUbo.glsl"

in vec2 fUV;

out vec4 fColor;

void main() {

    vec3 fboColor = texture(Fbo0, fUV).rgb;
	float shadow = texture(unShadowFbo, fUV).r;
	float shadowMult = shadow * 0.5 * SunHeight; // TODO: Move sun height out?
	//shadowMult = shadow;
	fColor.rgb = fboColor * shadowMult * ShadowColor + fboColor * (1.0 - shadowMult);
	fColor.a = 1.0;
}