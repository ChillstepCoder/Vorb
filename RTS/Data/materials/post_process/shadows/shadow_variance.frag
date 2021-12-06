//uniform sampler2D Fbo0;
uniform sampler2D FboDepth;
uniform sampler2DArray ShadowMap;
uniform vec3 ShadowColor;
#include "../../GlobalUbo.glsl"

uniform float ShadowCascadePlaneDistances[4];
uniform mat4 ShadowFrustumMatrices[4];
uniform vec3 CameraOffset;

in vec2 fUV;

const int cascadeCount = 4;

out vec4 fColor;

// TODO: SHARED
vec4 viewPosFromDepth(float depth, vec2 fboUV) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(fboUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = InverseP * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition;
}

// For reducing light bleed
float linstep(float low, float high, float v) {
	return clamp((v - low) / (high - low), 0.0, 1.0);
}

// https://www.youtube.com/watch?v=LGFDifcbsoQ
vec2 getShadowVariance(vec3 projCoords, int layer) {

	// get depth of current fragment from light's perspective
	float currentDepth = projCoords.z;
	//if (currentDepth  > 1.0) {
	//	return 0.0;
	//}

	// Calc moments and variance
	vec2 moments = texture(ShadowMap, vec3(projCoords.xy, layer)).rg;
	float p = step(currentDepth, moments.x);
	float variance = max(moments.y - moments.x * moments.x, 0.00002);
	
	// Chebychevs inequality
	float d = currentDepth - moments.x; // Distance from the mean
	float pMax = variance / (variance + d * d); // maximum percentage of values greater than equal to currentDepth
	// Reduce light bleeding hack
	pMax = linstep(0.8, 1.0, pMax);

	return vec2(1.0 - min(max(p, pMax), 1.0), d);
	//float pixelDepth = texture(ShadowMap, vec3(projCoords.xy, layer)).r;
	//return currentDepth > pixelDepth ? 1.0 : 0.0; 
}

vec2 getShadow(vec4 viewSpacePosition) {
	vec4 worldSpacePosition = InverseV * viewSpacePosition + vec4(CameraOffset, 0.0);
    float depthValue = abs(viewSpacePosition.z);
	
	int layer = cascadeCount;
    for (int i = 0; i < cascadeCount; ++i) {
	    // This branch is fine because local kernel will all follow same path usually
		if (depthValue < ShadowCascadePlaneDistances[i]) {
			layer = i;
			break;
		}
	}
	
	// Get the position of our fragment relative to the light view
	vec4 fragPosLightSpace = ShadowFrustumMatrices[layer] * vec4(worldSpacePosition.xyz, 1.0);
	
	// Remove shadow acne with bias
	// perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;
		
	vec2 shadowAndDist = getShadowVariance(projCoords, layer);
		
	// keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
	//if(projCoords.z > 1.0)
	//{
	//	shadow = 0.0;
	//}
	return shadowAndDist;
}

void main() {
    vec4 viewSpacePosition = viewPosFromDepth(texture(FboDepth, fUV).r, fUV);
	vec2 shadowAndDist = getShadow(viewSpacePosition);
	
	// Final color
    //fColor = texture(Fbo0, fUV);
	float shadowMult = shadowAndDist.x * 0.5 * SunHeight;
	// Multiply by ShadowColor to give more hue to shadows
	//fColor.rgb = fColor.rgb * shadowMult * ShadowColor + fColor.rgb * (1.0 - shadowMult);
	fColor.r = shadowMult;
	fColor.g = shadowAndDist.y;
	fColor.a = 1.0;
	//step(88.0, -viewSpacePosition.z)
}