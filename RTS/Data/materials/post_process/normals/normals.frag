uniform sampler2DArray Atlas;
uniform float Time;
uniform vec4 NoiseUvRect;
uniform float NoisePage;

in vec2 fUV;

out vec4 fColor;

void main() {
	vec2 texUv = fUV * NoiseUvRect.zw + NoiseUvRect.xy;
    fColor = texture(Atlas, vec3(texUv, NoisePage));
    fColor.rgb = (sin(fColor.rgb * 2.0) + 1.0) * 0.5 * 0.03;
    // fColor.a = (sin(Time) + 1.0) * 0.5;
    fColor.a = 0.02;
}