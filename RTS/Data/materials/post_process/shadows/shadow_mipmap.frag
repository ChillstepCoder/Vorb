
uniform sampler2D unInputTexture;
uniform int unPreviousLevel;

in vec2 fUV;

out vec4 fColor;

// Gaussian kernel
const float kernel[5][5] = {
 {1.0 / 256.0,  4.0 / 256.0,  6.0 / 256.0,  4.0 / 256.0, 1.0 / 256.0},
 {4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0},
 {6.0 / 256.0, 24.0 / 256.0, 36.0 / 256.0, 24.0 / 256.0, 6.0 / 256.0},
 {4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0},
 {1.0 / 256.0,  4.0 / 256.0,  6.0 / 256.0,  4.0 / 256.0, 1.0 / 256.0},
};

void main() {
	float sum = 0.0;
	float num = 0.0;
	fColor.r = 0.0; // Stores results of filtered
	// 5x5 box filter
	vec2 size = textureSize(unInputTexture, unPreviousLevel);
	vec2 texelOffset = 1.0 / size;
	for (int y = -2; y <= 2; ++y) {
		for (int x = -2; x <= 2; ++x) {
		    vec2 uv = vec2(fUV.x + texelOffset.x * x, fUV.y + texelOffset.y * y);
			vec3 val = textureLod(unInputTexture, uv, unPreviousLevel).rgb;
			if (val.g == 1.0) {
			  sum += val.b;
			  ++num;
			}
			fColor.r += val.r * kernel[x + 2][y + 2]; // TODO: what kernel?
		}
	}
	if (num > 0.0) {
	  fColor.b = sum / num;
	  fColor.g = 1.0;
	} else {
	  fColor.b = 0xFFFFFFFF;
	  fColor.g = 0.0;
	}
	fColor.a = 1.0;
}