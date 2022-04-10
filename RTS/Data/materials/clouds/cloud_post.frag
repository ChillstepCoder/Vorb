uniform sampler2DArray Atlas;
uniform sampler2D CloudFbo;
uniform sampler2D PreturbTexture;
uniform sampler2D unGradientTexture;
#include "../GlobalUbo.glsl"

uniform vec4 unCloudTextureRect;
uniform float unCloudTexturePage;

uniform float DebugFloat2;

#include "../util/lighting.glsl"
#include "../util/hsv.glsl"

in vec2 fUV;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;
layout (location = 2) out vec4 fRoughness;

void main() {
    vec4 cloudTextureSample = texture2D(CloudFbo, fUV);
	float baseAlpha = cloudTextureSample.a;
	//vec3 norm = normalize(blur13noalpha(CloudFbo, fUV, ScreenResolution, vec2(baseAlpha * 3.0, 0.0)));
	vec3 norm = cloudTextureSample.rgb;
	
	float z = step(0.000001, norm.z);
	if (z == 0.0) {
	  discard;
	}
    norm = normalize(norm * 2.0 - 1.0);
	
    
    // Get alpha
	vec2 tex = vec2(0.0, max(computeDiffuse(norm, SunPosition), 0.001));
	tex.y = 1.0 - tex.y;
	fColor.a = texture(Atlas, vec3(unCloudTextureRect.xy + tex * unCloudTextureRect.zw, unCloudTexturePage)).a * baseAlpha;
	
    // =========START TOON SHADING==========
	
    vec3 highlightColor = vec3(1.0);
    vec3 whiteColor = vec3(0.9);
    vec3 greyColor = vec3(0.83);
    vec3 darkGreyColor = vec3(0.7356);
    vec3 blackColor = vec3(0.5);
    
    vec3 screenNorm = (V * vec4(norm, 1.0)).xyz;
    // Tweak the normal by the position offset to make it more properly aligned
    screenNorm.xy += (fUV * 2.0 - 1.0);
    // Compute preturb texture UV
    vec2 preturbUV = (screenNorm.xy * 2.0 + 1.0);
    vec3 preturb = texture(PreturbTexture, preturbUV * 0.4).rgb * 0.00001;
    
    //norm += vec3(preturb.r * 2.0 - 0.5) * 0.25;
    norm = normalize(vec3(norm.x, norm.y, norm.z));
    // Front
   // float frontDiffuse = computeDiffuse(norm, SunPosition);
   // float highlightStep = step(1.0 - frontDiffuse, 0.4);
   // float whiteStep = step(1.0 - frontDiffuse, 0.99);
   // vec3 color = mix(greyColor, whiteColor, whiteStep);
   // color = mix(color, highlightColor, highlightStep);
    // Back
   // float backDiffuse = computeDiffuse(-norm, SunPosition);
   // float greyStep = step(1.0 - backDiffuse, 0.305);
   // color = mix(color, darkGreyColor, greyStep);
    //float blackStep = step(1.0 -y backDiffuse, 0.02);
    //color = mix(color, blackColor, blackStep);
    
   // fColor.rgb += color;
   
  
    
    // Color jitter
    vec3 hsv = rgb2hsv(fColor.rgb);
    //hsv.g = 0.037; // Saturation
    hsv.r = preturb.g + preturb.r; // Hue shift
    fColor.rgb = hsv2rgb(hsv);
    
       // New talia stuff
    float gradientAlpha = (dot(norm, SunPosition) + 1.0) * 0.5;
    vec2 newUV = vec2(0.5, 1.0 - gradientAlpha);
    // Uncomment for standard lighting
    fColor.rgb = fColor.rgb * 0.0001 + texture(unGradientTexture, newUV).rgb;
    
    // Uncomment to render normals
    //fColor.rgb = fColor.rgb * 0.00001 + (norm + 1.0) * 0.5;
    
    // Uncomment to render preturb
    //fColor.rgb = fColor.rgb * 0.00001 + preturb;
    
    // Uncomment to render preturb uv
    //fColor.rgb = fColor.rgb * 0.00001 + vec3(preturbUV.x, preturbUV.y, 0.0);
    
  
    
    //fColor.rgb = 0.00001 * fColor.rgb + gradientAlpha;
    // =========END TOON SHADING==========
    
    // Normals are up for main pass lighting
    fNormal = vec4(0.5, 0.5, 1.0, 1.0);
    fNormal.a = 1.0;
    
	fRoughness.r = 0.9;
	fRoughness.a = 1.0;
}