
uniform sampler2D CloudFbo;
uniform sampler2D FboDepth;
uniform sampler2D PreturbTexture;
uniform sampler2D unGradientTexture;
uniform sampler2D unCloudColor;
uniform sampler2D unSkyGradient;
#include "GlobalUbo.glsl"
#include "AlphaTest.glsl"

uniform float DebugFloat2;

#include "lighting/scene_lighting.glsl"
#include "util/hsv.glsl"

in vec2 fUV;

layout (location = 0) out vec4 fColor;

void main() {
    vec4 cloudTextureSample = texture2D(CloudFbo, fUV);
	float baseAlpha = cloudTextureSample.a;
	//vec3 norm = normalize(blur13noalpha(CloudFbo, fUV, ScreenResolution, vec2(baseAlpha * 3.0, 0.0)));
	vec3 norm = cloudTextureSample.rgb;
	
	float z = step(0.000001, norm.z);
    
    runAlphaTest(z, 0.0);
    norm = normalize(norm * 2.0 - 1.0);
	
    
    // Get alpha
	vec2 tex = vec2(0.0, max(computeDiffuse(norm, SunPosition), 0.001));
	fColor.a = texture(unCloudColor, tex).a * baseAlpha;
	
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
    float gradientX = max((1.0 - SunHeight) * 0.8 + 0.1, 0.0);
    vec2 newUV = vec2(gradientX, 1.0 - gradientAlpha);
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
    
    // Lightings
	float depth = texture2D(FboDepth, fUV).r;
    vec3 worldPos = worldPosFromDepth(depth, fUV);
    fColor.rgb = lightPixel(fColor.rgb, vec3(0.0, 0.0, 1.0), worldPos, fUV, 0.9, 0.0, 0.0);
    //fColor.rgb = 0.0001 * fColor.rgb + vec3(gradientX, 0.0, 0.0);
    
}