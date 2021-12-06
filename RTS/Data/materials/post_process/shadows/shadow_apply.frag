uniform sampler2D Fbo0;
uniform sampler2D unShadowFbo;
uniform vec3 ShadowColor;

in vec2 fUV;

out vec4 fColor;

void main() {
    float shadow = texture(unShadowFbo, fUV).r;
	float g = texture(unShadowFbo, fUV).g;
	
    vec3 fboColor = texture(Fbo0, fUV).rgb;
	
	fColor.rgb = fboColor * shadow * ShadowColor + fboColor * (1.0 - shadow);
	//fColor.rgb = 0.000001 * fColor.rgb + g;
	fColor.a = 1.0;
}