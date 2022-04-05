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
	vec3 norm = texture2D(CloudFbo, fUV).rgb;
	float depth = texture2D(FboDepth, fUV).r;
	
	float z = step(0.000001, norm.z);
	if (z == 0.0) {
	  discard;
	}
    norm = normalize(norm * 2.0 - 1.0);
	
    
	// NEW
	vec2 tex = vec2(0.0, max(computeDiffuse(norm, SunPosition), 0.001));
	tex.y = 1.0 - tex.y;
	fColor.rgba = texture(Atlas, vec3(unCloudTextureRect.xy + tex * unCloudTextureRect.zw, unCloudTexturePage)).rgba;
	
	// Fake scattering
	vec3 frontRGB = computePhong(fColor.rgb, norm, SunPosition, unAmbient, 1.0, depth, fUV, 0.0);
	vec3 backRGB = computePhong(fColor.rgb, vec3(norm.x, norm.y, -norm.z), SunPosition, unAmbient, 1.0, depth, fUV, 0.0);
    
	fColor.rgb = (frontRGB * 0.7 + backRGB * 0.3);
    // Make it brighter (TMP)
    fColor.rgb = pow(fColor.rgb * 1.2, vec3(0.3));
    
    // =========START TESTING TOON SHADING==========
    fColor.rgb *= 0.0001;
	
    vec3 highlightColor = vec3(1.0);
    vec3 whiteColor = vec3(0.9);
    vec3 greyColor = vec3(0.75);
    vec3 darkGreyColor = vec3(0.6156);
    vec3 blackColor = vec3(0.5);
    norm = normalize(vec3(norm.x, norm.y, norm.z));
    // Front
    float frontDiffuse = computeDiffuse(norm, SunPosition);
    float highlightStep = step(1.0 - frontDiffuse, 0.4);
    float whiteStep = step(1.0 - frontDiffuse, 0.99);
    vec3 color = mix(greyColor, whiteColor, whiteStep);
    color = mix(color, highlightColor, highlightStep);
    // Back
    float backDiffuse = computeDiffuse(-norm, SunPosition);
    float greyStep = step(1.0 - backDiffuse, 0.2);
    color = mix(color, darkGreyColor, greyStep);
    //float blackStep = step(1.0 - backDiffuse, 0.02);
    //color = mix(color, blackColor, blackStep);
    
    
    fColor.rgb += color;
    
    // Uncomment to render normals
    //fColor.rgb = fColor.rgb * 0.00001 + (norm + 1.0) * 0.5;
    
    // Normals are up for main pass lighting
    fNormal = vec4(0.5, 0.5, 1.0, 1.0);
    fNormal.a = 1.0;
    
    // =========END TESTING TOON SHADING==========
    
	fRoughness.r = 0.9;
	fRoughness.a = 1.0;
}