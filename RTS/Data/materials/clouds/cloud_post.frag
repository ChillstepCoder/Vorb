uniform sampler2DArray Atlas;
uniform sampler2D CloudFbo;
uniform sampler2D FboDepth;
uniform float unAmbient;
#include "../GlobalUbo.glsl"

uniform vec4 unCloudTextureRect;
uniform float unCloudTexturePage;

#include "../util/lighting.glsl"

in vec2 fUV;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;
layout (location = 2) out vec4 fRoughness;

void main() {
    // fColor = texture(CloudFbo, fUV);
	float baseAlpha = texture2D(CloudFbo, fUV).a;
	//vec3 norm = normalize(blur13noalpha(CloudFbo, fUV, ScreenResolution, vec2(baseAlpha * 3.0, 0.0)));
	vec3 norm = normalize(texture2D(CloudFbo, fUV).rgb * 2.0 - 1.0);
    vec3 TMPNORM = norm;
	float depth = texture2D(FboDepth, fUV).r;
	
	float z = step(0.000001, norm.z);
	if (z == 0.0) {
	  discard;
	}
	
    // DISABLE NORMALS
	//fNormal = vec4((norm + 1.0) * 0.5, 1.0);
    fNormal = vec4(0.5, 0.5, 0.5, 1.0);
    
	// OLD
	//vec2 tex = ((norm.xy + vec2(1.0)) * 0.5);
	//tex.y = 1.0 - tex.y;
	//fColor.rgba = texture(Atlas, vec3(unCloudTextureRect.xy + tex * unCloudTextureRect.zw, unCloudTexturePage)).rgba;
	
	// NEW
	vec2 tex = vec2(0.0, max(computeDiffuse(norm, SunPositionCameraRelative), 0.001));
	tex.y = 1.0 - tex.y;
	fColor.rgba = texture(Atlas, vec3(unCloudTextureRect.xy + tex * unCloudTextureRect.zw, unCloudTexturePage)).rgba;
	
	// Fake scattering
	vec3 frontRGB = computePhong(fColor.rgb, norm, SunPositionCameraRelative, unAmbient, 1.0, depth, fUV, 0.0);
	vec3 backRGB = computePhong(fColor.rgb, vec3(norm.x, norm.y, -norm.z), SunPositionCameraRelative, unAmbient, 1.0, depth, fUV, 0.0);
    
	fColor.rgb = (frontRGB * 0.7 + backRGB * 0.3);
    // Make it brighter (TMP)
    fColor.rgb = pow(fColor.rgb * 1.2, vec3(0.3));
    
    // =========START TESTING TOON SHADING==========
    fColor.rgb *= 0.0001;
	
    vec3 whiteColor = vec3(1.0);
    vec3 greyColor = vec3(0.6156);
    norm = normalize(vec3(norm.x, norm.y, norm.z * 0.4));
    float colorStep = step(1.0 - computeDiffuse(norm, SunPositionCameraRelative), 0.5);
    vec3 color = colorStep * whiteColor + (1.0 - colorStep) * greyColor;
    fColor.rgb = fColor.rgb + color;
    
    // TODO: REMOVE
    fColor.rgb = fColor.rgb * 0.00001 + (TMPNORM + 1.0) * 0.5;
    fColor.rg = vec2(1.0);
    
    
    // =========END TESTING TOON SHADING==========
    
	fRoughness.r = 0.9;
	fRoughness.a = 1.0;
}