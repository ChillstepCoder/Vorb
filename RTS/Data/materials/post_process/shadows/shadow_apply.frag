uniform sampler2D unShadowFbo;
uniform int unMipCount;

in vec2 fUV;

out vec4 fColor;

const int MIP_COUNT = 9; // TODO: Enforce in code

void main() {

	float shadow = 0.0;
	vec3 val[MIP_COUNT];
	// Fetch mipmap levels
	for (int level = 0; level < MIP_COUNT; ++level) {
	    val[level] = textureLod(unShadowFbo, fUV, level).rgb;
	}
	// Find penumbra width
	float q = 0;
	// Skip first level
	for (int level = 1; level < MIP_COUNT && q == 0; ++level) {
	  if (val[level].g == 1.0) q = val[level].b;
	}
	
	// Select penumbra levels
	int down;
	if (q > 0.0) {
	  if (q < 1.0) q = 1.0;
	  float l = log2(q);
	  if (l > MIP_COUNT - 1) l = MIP_COUNT - 1;
	  down = int(floor(l));
	  int up = down + 1;
	  float interp = l - down;
	  shadow = (mix(val[down].r, val[up].r, interp));
	}

	
	fColor.r = shadow;
	fColor.a = 1.0;
}