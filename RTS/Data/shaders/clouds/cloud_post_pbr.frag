
uniform sampler2D unCloudTexture;
uniform sampler2D unDepthTexture;
uniform sampler2D PreturbTexture;
uniform sampler2D unGradientTexture;
uniform sampler2D unCloudColor;
uniform sampler2D unSkyGradient;
uniform sampler2D GradientTexture;
uniform float unCloudMetallic;
uniform float unCloudRoughness;


uniform float DebugFloat2;
uniform vec2 unGamma;
uniform vec2 unAmbient;
uniform vec2 unSunIntensity;
uniform vec2 unExposure;
uniform float unLightingSplit;

// for ComputeDiffuse

#include "GlobalUbo.glsl"
#include "AlphaTest.glsl"
#include "util/lighting.glsl"
#include "util/ambient.glsl"
#include "util/gamma.glsl"
#include "util/pbr.glsl"
#include "util/haze.glsl"
#include "util/hsv.glsl"
#include "util/depth.glsl"

in vec2 fUV;

layout (location = 0) out vec4 oColor;

vec3 getCurrentSunColor(int preset, vec3 sunColor, float sunIntensity) {
	float sunIntensityAdjusted = max(sunIntensity * unSunIntensity[preset], 0.0);
	float lightTotal = sunIntensityAdjusted;// + unAmbient[preset];
    return lightTotal * sunColor;
}

vec3 getAdditiveFromSun(float sunVal, float sunAngle, vec3 color, float size) {
    // Glow
    float additive = pow(sunAngle, 4.0) * max(pow(sunVal, 1.0 - size), 0.0);
	// Sky sun glow + sun texture
    return additive * color;
}


void main() {
    const int preset = int(step(unLightingSplit, fUV.x));
    
    vec4 cloudTextureSample = texture2D(unCloudTexture, fUV);
	float baseAlpha = cloudTextureSample.a;
	//vec3 norm = normalize(blur13noalpha(unCloudTexture, fUV, ScreenResolution, vec2(baseAlpha * 3.0, 0.0)));
	vec3 norm = cloudTextureSample.rgb;
	
	float z = step(0.000001, norm.z);
    
    runAlphaTest(z, 0.0);
    norm = normalize(norm * 2.0 - 1.0);
	
    
    // Get alpha
	vec2 tex = vec2(0.0, max(computeDiffuse(norm, SunPosition), 0.001));
	oColor.a = texture(unCloudColor, tex).a * baseAlpha;
	
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
    
   // oColor.rgb += color;
   
  
    
    // Color jitter
    vec3 hsv = rgb2hsv(oColor.rgb);
    //hsv.g = 0.037; // Saturation
    hsv.r = preturb.g + preturb.r; // Hue shift
    oColor.rgb = hsv2rgb(hsv);
    
       // New talia stuff
    float gradientAlpha = (dot(norm, SunPosition) + 1.0) * 0.5;
    float gradientX = max((1.0 - SunHeight) * 0.8 + 0.1, 0.0);
    vec2 newUV = vec2(gradientX, 1.0 - gradientAlpha);
    // Uncomment for standard lighting
    oColor.rgb = oColor.rgb * 0.0001 + gammaDecode(texture(unGradientTexture, newUV).rgb, unGamma[preset]);
    
    // Uncomment to render normals
    //oColor.rgb = oColor.rgb * 0.00001 + (norm + 1.0) * 0.5;
    
    // Uncomment to render preturb
    //oColor.rgb = oColor.rgb * 0.00001 + preturb;
    
    // Uncomment to render preturb uv
    //oColor.rgb = oColor.rgb * 0.00001 + vec3(preturbUV.x, preturbUV.y, 0.0);
    
  
    
    //oColor.rgb = 0.00001 * oColor.rgb + gradientAlpha;
    // =========END TOON SHADING==========
    
    // Lightings
	float depth = texture2D(unDepthTexture, fUV).r;
    vec3 worldPos = worldPosFromDepth(depth, fUV, InverseV, InverseP);
    
    // PBR
    vec3 sunColor = getCurrentSunColor(preset, SunColor, SunHeight);
    
    
    // Cloud stays bright longer than terrain on sunset
    float ambientSunHeight = min(SunHeight + 0.2, 1.0);
    
    float ambient = getAmbientFactor(preset, unAmbient, ambientSunHeight);
    oColor.rgb = PBR(worldPos, oColor.rgb, vec3(0.0, 0.0, 1.0), unCloudMetallic, unCloudRoughness, 1.0, 0.0, ambient, unExposure[preset], sunColor, SunPosition, vec3(0.0));
    
    // Haze (TODO: Final tonemap?)
	float sunIntensity = max(SunHeight, 0.0);
    vec3 hazeColor = texture(GradientTexture, vec2(0.5, sunIntensity)).rgb;
    oColor.rgb = applyHaze(oColor.rgb, worldPos, preset, hazeColor);
    
    // Sun shinethrough
    // World space ray
	vec4 rayClip = vec4(fUV.x * 2.0 - 1.0, fUV.y * 2.0 - 1.0, -1.0, 1.0);
	vec4 rayWorld = InverseVP * rayClip;
	// Get Angle
	float sunAngle = max(pow(dot(SunPosition, normalize(rayWorld.xyz)), 64.0), 0.0);
    oColor.rgb += getAdditiveFromSun(sunIntensity, sunAngle, sunColor, 0.1);
    
    // Gamma
    oColor.rgb = gammaEncode(oColor.rgb, unGamma[preset]);
    
    
    //oColor.rgb = lightPixel(oColor.rgb, vec3(0.0, 0.0, 1.0), worldPos, fUV, 0.9, 0.0, 0.0);
    //oColor.rgb = 0.0001 * oColor.rgb + vec3(gradientX, 0.0, 0.0);
    
}