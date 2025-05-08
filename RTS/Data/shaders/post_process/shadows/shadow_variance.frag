//uniform sampler2D Fbo0;
uniform sampler2D FboNormal;
uniform sampler2D FboDepth;
uniform sampler2DArray unShadowMap;
#include "GlobalUbo.glsl"
#include "util/depth.glsl"

uniform float unShadowCascadePlaneDistances[4];
uniform mat4 unShadowFrustumMatrices[4];
uniform vec3 CameraOffset;

in vec2 fUV;

const int cascadeCount = 4;

out vec3 fColor;


// For reducing light bleed
float linstep(float low, float high, float v) {
	return clamp((v - low) / (high - low), 0.0, 1.0);
}

// https://www.youtube.com/watch?v=LGFDifcbsoQ
vec2 getShadowVariance(vec3 projCoords, int layer, vec3 worldCoords, mat4 inverseLight) {

	// get depth of current fragment from light's perspective
	float currentDepth = projCoords.z * 2.0 - 1.0;
	//if (currentDepth  > 1.0) {
	//	return 0.0;
	//}

	// Calc moments and variance
	vec2 moments = texture(unShadowMap, vec3(projCoords.xy, layer)).rg;
	float p = step(currentDepth, moments.x);
	float variance = max(moments.y - moments.x * moments.x, 0.00002);
	
	// Chebychevs inequality
	float d = currentDepth - moments.x; // Distance from the mean
	float pMax = variance / (variance + d * d); // maximum percentage of values greater than equal to currentDepth
	// Reduce light bleeding hack
	pMax = linstep(0.98, 1.0, pMax);
	
	// Get world position of occluder
	float z = moments.x * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(projCoords.xy * 2.0 - 1.0, z, 1.0);
    vec4 occluderWorldSpacePos = inverseLight * clipSpacePosition;
	
	
    float distance = length(occluderWorldSpacePos.rgb - worldCoords);
	float shadow = 1.0 - min(max(p, pMax), 1.0);
	distance = clamp(distance, 0.0, 10.0);
	return vec2(shadow, distance);
}

vec2 getShadowAndDistAtLayer(int layer, vec3 worldSpacePosition) {
  // Get the position of our fragment relative to the light view
	vec4 fragPosLightSpace = unShadowFrustumMatrices[layer] * vec4(worldSpacePosition, 1.0);
	
	// Remove shadow acne with bias
	// perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;
	
	vec2 shadowAndDist = getShadowVariance(projCoords, layer, worldSpacePosition, inverse(unShadowFrustumMatrices[layer]));

	return shadowAndDist;
}

vec2 getShadow(vec4 viewSpacePosition, vec3 normal) {
	vec4 worldSpacePosition = InverseV * viewSpacePosition + vec4(CameraOffset, 0.0);
	// Bias with normals (This looks wrong! += is better but idk)
	// worldSpacePosition.xyz -= normal * 0.04;
    float depthValue = abs(viewSpacePosition.z);
	
	float dist = 0.0;
	int layer = cascadeCount;
    for (int i = 0; i < cascadeCount; ++i) {
	    // This branch is fine because local kernel will all follow same path usually
		if (depthValue < unShadowCascadePlaneDistances[i]) {
		    if (i > 0) {
			    // Store distance from previous plane so we can blend
		        dist = depthValue - unShadowCascadePlaneDistances[i - 1];
				dist = clamp(dist, 0.0, 10.0) / 10.0;
			}
			layer = i;
			break;
		}
	}
	
	if (layer == cascadeCount) {
		return vec2(0.0);
	} else {
		vec2 shadowAndDist = getShadowAndDistAtLayer(layer, worldSpacePosition.xyz);
		if (layer > 0) {
		   shadowAndDist = mix(getShadowAndDistAtLayer(layer - 1, worldSpacePosition.xyz), shadowAndDist, dist);
		}
	    return shadowAndDist;
	}
}

void main() {
	vec3 normal = texture(FboNormal, fUV).rgb * 2.0 - 1.0;
	//normal.z *= 0.0; // No Z bias
	float depth = texture(FboDepth, fUV).r;
    vec4 viewSpacePosition = viewPosFromDepth(depth, fUV, InverseP);
	vec2 shadowAndDist = getShadow(viewSpacePosition, normal);
	
	// Final color
    //fColor = texture(Fbo0, fUV);
	fColor.r = shadowAndDist.x;
	// TODO: remove?
	fColor.g = step(0.0001, shadowAndDist.x);
	fColor.b = shadowAndDist.y;
	//step(88.0, -viewSpacePosition.z)
}