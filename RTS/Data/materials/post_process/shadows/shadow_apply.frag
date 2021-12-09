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
	int level;
	for (level = 0; level < MIP_COUNT; ++level) {
	    if (val[level].g == 1.0) {
	        q = val[level].b;
			break;
	    }
	}
	float mult = 2.5;
	if (level == 0) {
	   q = 1.0;
	   mult = 1.0;
	}
	// Select penumbra levels
	int down;
	float interp;
	float l;
	if (q > 0.0) {
	  q = max(q, 1.0);
	  l = log2(q);
	  l = min(l, float(MIP_COUNT - 1));
	  down = int(floor(l));
	  int up = down + 1;
	  interp = l - down;
	  shadow = (mix(val[down].r, val[up].r, interp));
	}
	
	fColor.r = min(shadow * mult, 1.0);
	fColor.a = 1.0;
}