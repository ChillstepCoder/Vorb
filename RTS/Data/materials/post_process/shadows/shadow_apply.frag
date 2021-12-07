uniform sampler2D Fbo0;
uniform sampler2D unShadowFbo;
uniform vec3 ShadowColor;
uniform int unMipCount;

in vec2 fUV;

out vec4 fColor;

void main() {

	float shadow = 0.0;
	vec3 val[12];
	// Fetch mipmap levels
	for (int level = 0; level < unMipCount; ++level) {
	    val[level] = textureLod(unShadowFbo, fUV, level).rgb;
	}
	// Find penumbra width
	float q = 0;
	// Skip first level
	for (int level = 1; level < unMipCount && q == 0; ++level) {
	  if (val[level].g == 1.0) q = val[level].b;
	}
	int down;
	if (q > 0.0) {
	  if (q < 1.0) q = 1.0;
	  float l = log2(q);
	  if (l > unMipCount) l = unMipCount;
	  down = int(floor(l));
	  int up = down + 1;
	  float interp = l - down;
	  shadow = (mix(val[down].r, val[up].r, interp));
	}

    //float shadow = texture(unShadowFbo, fUV).r;
	 //shadow = textureLod(unShadowFbo, fUV, 0).g;
	//shadow = float(down) * 0.1;
	// shadow = textureLod(unShadowFbo, fUV, 1).b * 100.0;
	
    vec3 fboColor = texture(Fbo0, fUV).rgb;
	
	fColor.rgb = vec3(shadow);
	fColor.rgb = fboColor * shadow * ShadowColor + fboColor * (1.0 - shadow);
	//fColor.rgb = 0.000001 * fColor.rgb + g;
	fColor.a = 1.0;
}