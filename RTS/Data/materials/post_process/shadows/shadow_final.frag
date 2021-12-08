uniform sampler2D Fbo0;
uniform sampler2D unShadowFbo;
uniform vec3 ShadowColor;

in vec2 fUV;

out vec4 fColor;

void main() {

    vec3 fboColor = texture(Fbo0, fUV).rgb;
	float shadow = texture(unShadowFbo, fUV).r;
	
	fColor.rgb = fboColor * shadow * ShadowColor + fboColor * (1.0 - shadow);
	fColor.a = 1.0;
}