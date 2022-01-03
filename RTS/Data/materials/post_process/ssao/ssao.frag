// https://learnopengl.com/Advanced-Lighting/SSAO

uniform sampler2D FboDepth;
uniform sampler2D FboNormals;
uniform vec2 ScreenResolution;
uniform sampler2D unTexNoise;
uniform float unRadius;
uniform float unBias;
uniform float unOcclusionAdjust = 7.0;
const int KERNEL_SIZE = 32; // Match C++
uniform vec3 unSamples[KERNEL_SIZE];

#include "../../GlobalUbo.glsl"

in vec2 fUV;

const float NOISE_SCALE = 4.0;

out float fColor;

// TODO: Shared
vec4 viewPosFromDepth(float depth, vec2 fboUV, mat4 inverseP) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(fboUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = inverseP * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition;
}

float easeInOutCubic(float x) {
   return x < 0.5 ? (4.0 * x * x * x) : (1.0 - pow(-2.0 * x + 2.0, 3.0) / 2.0);
}

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
}

void main() {
    float depth = texture(FboDepth, fUV).r;
    vec3 viewPos = viewPosFromDepth(depth, fUV, InverseP).xyz;
    vec3 normal = texture(FboNormals, fUV).rgb * 2.0 - 1.0;
	vec2 noiseScale = ScreenResolution / NOISE_SCALE;
    vec3 randomVec = texture(unTexNoise, fUV * noiseScale).xyz;
   
    // Screen space
    normal = (V * vec4(normal, 1.0)).xyz;
   
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
	
	// Reduce AO far from the camera
	float depthBias = unBias * (linearizeDepth(depth) + 30.0) * 0.03;
   
    float occlusion = 0.0;
	for(int i = 0; i < KERNEL_SIZE; ++i)
	{
		// get sample position
		vec3 samplePos = TBN * unSamples[i]; // from tangent to view-space
		samplePos = viewPos + samplePos * unRadius;
		vec4 offset = vec4(samplePos, 1.0);
		offset = P * offset;    // from view to clip-space
		offset.xyz /= offset.w;  // perspective divide
		offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
		float sampleDepth = texture(FboDepth, offset.xy).z;
		vec3 viewPosSample = viewPosFromDepth(sampleDepth, offset.xy, InverseP).xyz;
		
		float rangeCheck = smoothstep(0.0, 1.0, unRadius / abs(viewPos.z - viewPosSample.z));
		occlusion += (viewPosSample.z > samplePos.z + depthBias ? 1.0 : 0.0) * rangeCheck;
	}
	occlusion = 1.0 - (occlusion / (float(KERNEL_SIZE) - unOcclusionAdjust));
	//occlusion = pow(occlusion, 0.5); // Whiten the whites
	occlusion = easeInOutCubic(occlusion);
	occlusion = pow(occlusion, 2.5);
    occlusion = clamp(occlusion, 0.45, 1.0);
    fColor = occlusion;  
}