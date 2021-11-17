uniform sampler2D Fbo0;
uniform sampler2D FboDepth;
uniform sampler2D FboNormals;
uniform sampler2DArray ShadowMap;
uniform vec3 SunPosition;
uniform mat4 InverseP;
uniform mat4 InverseV;
uniform vec2 CameraZRange;
uniform float SunHeight;

uniform float ShadowCascadePlaneDistances[4];
uniform mat4 ShadowFrustumMatrices[4];

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

float getShadow(vec4 viewSpacePosition) {
	vec4 worldSpacePosition = InverseV * viewSpacePosition;
    float depthValue = abs(viewSpacePosition.z);
	
	int layer = cascadeCount;
    for (int i = 0; i < cascadeCount; ++i) {
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
		
	// get depth of current fragment from light's perspective
	float currentDepth = projCoords.z;
	if (currentDepth  > 1.0) {
		return 0.0;
	}
	// calculate bias (based on depth map resolution and slope)
	
	vec3 normal = texture(FboNormals, fUV).rgb * 2.0 - 1.0;
	float layerBiasMult = pow(layer, 4.0) * 0.0024; // More bias further from camera
	float bias = max((0.02 + layerBiasMult) * (1.0 - dot(normal, SunPosition)), 0.005);
	if (layer == cascadeCount) {
		bias *= 1 / (CameraZRange.y * 0.5);
	}
	else {
		bias *= 1 / (ShadowCascadePlaneDistances[layer] * 0.5);
	}
	
	
	// PCF
	float shadow = 0.0;
	vec2 texelSize = 1.0 / vec2(textureSize(ShadowMap, 0));
	for(int x = -1; x <= 1; ++x) {
		for(int y = -1; y <= 1; ++y) {
			float pcfDepth = texture(
						ShadowMap,
						vec3(projCoords.xy + vec2(x, y) * texelSize, layer)
						).r; 
			shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;        
		}    
	}
	shadow /= 9.0;
	//shadow = texture(ShadowMap, vec3(projCoords.xy, layer)).r;
	//cshadow = (currentDepth - bias) > shadow ? 1.0 : 0.0;      
		
	// keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
	if(projCoords.z > 1.0)
	{
		shadow = 0.0;
	}
	return shadow;
}

void main() {
    vec4 viewSpacePosition = viewPosFromDepth(texture(FboDepth, fUV).r, fUV);
	float shadow = getShadow(viewSpacePosition);
	
	// Final color
    fColor = texture(Fbo0, fUV);
	fColor.rgb = fColor.rgb * (1.0 - shadow * 0.5 * SunHeight);
	//step(88.0, -viewSpacePosition.z)
}