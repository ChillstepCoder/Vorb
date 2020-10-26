uniform float InnerRadius;
uniform float OuterRadius;
uniform vec3 Color;
uniform float Intensity;

in vec2 fLocalPosition;
out vec4 fColor;

void main() {
    float radialDistance = sqrt(fLocalPosition.x * fLocalPosition.x + fLocalPosition.y * fLocalPosition.y);
	// Scale between 0 and 1
	// Comments are a simple light model but less realistic
	float adjustedDistance = mix(InnerRadius, OuterRadius, radialDistance) / (OuterRadius - InnerRadius);
	adjustedDistance = clamp(adjustedDistance, 0.0, 1.0);
	
	// Realistic lighting attenuation
	// https://www.wolframalpha.com/input/?i=graph+%28%28+%281.0+%2F+%281.0+%2B+x*+x+*+8%29%29+-+0.1111111%29%29+*+1.11111111+from+0+to+1
	float attenuatedIntensity = ((1.0 / (1.0 + adjustedDistance * adjustedDistance * 8.0)) - 0.11111111) * 1.11111111;
	
	float finalIntensity = attenuatedIntensity * Intensity;
    fColor.rgb = Color * finalIntensity;
	fColor.a = finalIntensity;
}