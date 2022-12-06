uniform sampler2D FboNormals;
uniform sampler2D FboDepth;
uniform float InnerRadius;
uniform vec3 Color;
uniform float Intensity;

in vec2 fLocalPosition;
in vec2 fUvNormals;

out vec4 fColor;

const float LIGHT_HEIGHT = 0.05;
const float DEPTH_BIAS = 1000.0;

void main() {
    float radialDistance = sqrt(fLocalPosition.x * fLocalPosition.x + fLocalPosition.y * fLocalPosition.y);
	// Scale between 0 and 1
	// Comments are a simple light model but less realistic
	float adjustedDistance = (radialDistance - InnerRadius) / (1.0f - InnerRadius);
	adjustedDistance = clamp(adjustedDistance, 0.0, 1.0);
	
	// Realistic lighting attenuation
	// https://www.wolframalpha.com/input/?i=graph+%28%28+%281.0+%2F+%281.0+%2B+x*+x+*+8%29%29+-+0.1111111%29%29+*+1.11111111+from+0+to+1
	float attenuatedIntensity = ((1.0 / (1.0 + adjustedDistance * adjustedDistance * 8.0)) - 0.11111111) * 1.11111111;
	
	// Get surface normal
	vec3 normal = (texture(FboNormals, fUvNormals).rgb * 2.0) - 1.0;
	
	// Adjust normal by depth
	float depth = texture(FboDepth, fUvNormals).r;
	float depthBelow = textureOffset(FboDepth, fUvNormals, ivec2(0, 1)).r;
	normal.y += (depth - depthBelow) * DEPTH_BIAS;
	normal = normalize(normal);
	
	// Compute light from dot product
	vec3 lightDir = normalize(vec3(fLocalPosition, LIGHT_HEIGHT));
	float normalIntensity = max(dot(normal, lightDir), 0.0);
	
	// Aggregate it all
	float finalIntensity = attenuatedIntensity * Intensity * normalIntensity;
	fColor.rgb = Color * finalIntensity;
	fColor.a = attenuatedIntensity * Intensity;
}