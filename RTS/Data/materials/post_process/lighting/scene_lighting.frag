uniform sampler2DArray Atlas;
uniform sampler2D Fbo0;
uniform sampler2D FboNormals;
uniform sampler2D FboDepth;
uniform sampler2D FboRoughness;
uniform sampler2D ShadowTexture;
uniform sampler2D SSAOTexture;
uniform vec4 GradientRect;
uniform float GradientAtlasPage;
uniform vec3 ShadowColor;
uniform vec3 SSAOColor;
uniform float unGamma;
uniform float unExposure;

uniform int unTonemapOperator;
uniform int unLightingModel;

#include "../../GlobalUbo.glsl"


#include "../../util/lighting.glsl"

in vec2 fUV;

out vec4 fColor;

vec3 tonemapFilmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}

vec3 tonemapUnreal(vec3 x) {
  return x / (x + 0.155) * 1.019;
}

vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
  vec3 w2 = vec3(step(m + l0, x));
  vec3 w1 = vec3(1.0 - w0 - w2);

  vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
  vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
  vec3 L = vec3(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}

vec3 tonemapUchimura(vec3 x) {
  const float P = 1.0;  // max display brightness
  const float a = 1.0;  // contrast
  const float m = 0.22; // linear section start
  const float l = 0.4;  // linear section length
  const float c = 1.33; // black
  const float b = 0.0;  // pedestal

  return uchimura(x, P, a, m, l, c, b);
}

vec3 tonemapLottes(vec3 x) {
  const vec3 a = vec3(1.6);
  const vec3 d = vec3(0.977);
  const vec3 hdrMax = vec3(8.0);
  const vec3 midIn = vec3(0.18);
  const vec3 midOut = vec3(0.267);

  const vec3 b =
      (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
  const vec3 c =
      (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

  return pow(x, a) / (pow(x, a * d) * b + c);
}

vec3 uncharted2Tonemap(vec3 x) {
  float A = 0.15;
  float B = 0.50;
  float C = 0.10;
  float D = 0.20;
  float E = 0.02;
  float F = 0.30;
  float W = 11.2;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(vec3 color) {
  const float W = 11.2;
  float exposureBias = unExposure * 2.0;
  vec3 curr = uncharted2Tonemap(exposureBias * color);
  vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
  return curr * whiteScale;
}

vec3 tonemapReinhard2(vec3 x) {
  const float L_white = unExposure * 4.0;

  return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}


vec3 computeTonemapping(vec3 inColor) {

    switch (unTonemapOperator) {
        case 1:
            // reinhard tone mapping
            return tonemapReinhard2(inColor);
        case 2:
            return tonemapLottes(inColor);
        case 3:
            return tonemapUchimura(inColor);
        case 4:
            return tonemapUnreal(inColor);
        case 5:
            return tonemapFilmic(inColor);
        case 6:
            return tonemapUncharted2(inColor);
    }
    return inColor;
}


const float AMBIENT = 0.1;


void main() {

	float depth = texture(FboDepth, fUV).r;
	float isSky = step(0.999999999, depth);
	float isGround = 1.0 - isSky;
	float shadow = texture(ShadowTexture, fUV).r;
	float isShadow = step(0.0001, shadow);
	
	// =====================================================
	// ==                     Sunlight color              ==
	// =====================================================
	
	vec3 fboColor = texture(Fbo0, fUV).rgb;
	float hazeIntensity = max(SunHeight, 0.0);
	float sunIntensity = hazeIntensity * (1.0 - AMBIENT);
	float lightTotal = sunIntensity + AMBIENT;
    // Clamp sky light total
    if (isSky > 0.0) {
        if (unTonemapOperator > 0.0) {
            fColor.rgb = lightTotal * SunColor * fboColor;
        } else {
            fColor.rgb = (min(lightTotal, 0.3) * SunColor) * fboColor;
        }
    } else {
        fColor.rgb = lightTotal * SunColor * fboColor;
    }

	
	// =====================================================
	// ==                     SUN                         ==
	// =====================================================
	// World space ray
	vec4 rayClip = vec4(fUV.x * 2.0 - 1.0, fUV.y * 2.0 - 1.0, -1.0, 1.0);
	vec4 rayWorld = InverseVP * rayClip;
	// Get Angle
	float sunAngle = max(pow(dot(SunPosition, normalize(rayWorld.xyz)), 64.0), 0.0);
	
	// Sun Glow
	fColor.rgb += sunAngle * max(pow(hazeIntensity, 0.5) - 0.1, 0.0);
	// Sky sun glow + sun texture
	fColor.rgb += isSky * (sunAngle * 0.5 + max(pow(sunAngle - 0.95, 0.3), 0.0) * 2.0);
	
	// Sun phong
	vec3 normal = texture(FboNormals, fUV).rgb;
	normal = normal * 2.0 - 1.0;
	float roughness = texture(FboRoughness, fUV).r;
	roughness = max(roughness, isSky);
    
    float PHONG_AMBIENT = 0.5;
    if (unTonemapOperator > 0.0) {
        if (unLightingModel == 0) {
            // Phong
            fColor.rgb = computePhongHDR(fColor.rgb, normal, SunPosition, PHONG_AMBIENT, roughness, depth, fUV, shadow);
        } else {
            // Blinn phong
            fColor.rgb = computeBlinnPhongHDR(fColor.rgb, normal, SunPosition, PHONG_AMBIENT, roughness, depth, fUV, shadow);
        }
    } else {
	     if (unLightingModel == 0) {
            // Phong
            fColor.rgb = computePhong(fColor.rgb, normal, SunPosition, PHONG_AMBIENT, roughness, depth, fUV, shadow);
        } else {
            // Blinn phong
            fColor.rgb = computeBlinnPhong(fColor.rgb, normal, SunPosition, PHONG_AMBIENT, roughness, depth, fUV, shadow);
        }
    }
	
	// =====================================================
	// ==                     SHADOW                      ==
	// =====================================================
	float shadowMult = shadow * 0.5 * SunHeight;
	fColor.rgb = fColor.rgb * shadowMult * ShadowColor + fColor.rgb * (1.0 - shadowMult);
	
	// =====================================================
	// ==                     SSAO                        ==
	// =====================================================
	//float ssao = texture(SSAOTexture, fUV).r;
	//fColor.rgb = mix(SSAOColor, fColor.rgb, ssao);
	//fColor.rgb = fColor.rgb * 0.00001 + vec3(ssao);
    
    // =====================================================
	// ==                     HAZE                        ==
	// =====================================================
	float adjustedDepth = pow(depth, 3500.0);
	float depthHaze = min(adjustedDepth * (pow(hazeIntensity, 0.5)), 1.0);
	vec2 adjustedUV = fUV;
	adjustedUV.y = hazeIntensity;
	vec3 sunTextureColor = texture(Atlas, vec3(GradientRect.xy + adjustedUV * GradientRect.zw, GradientAtlasPage)).rgb;
	// Day Haze
	fColor.rgb = fColor.rgb * (1.0 - depthHaze * isGround) + depthHaze * sunTextureColor;

	// Night Haze
	float nightHaze = 1.0 - adjustedDepth * isGround * (1.0 - pow(hazeIntensity, 0.2));
	fColor.rgb *= nightHaze;
    
    
    // =====================================================
	// ==                     HDR TONE MAPPING            ==
	// =====================================================
    fColor.rgb = computeTonemapping(fColor.rgb);
    // Gamma correction
    fColor.rgb = pow(fColor.rgb, vec3(1.0 / unGamma));
    
	fColor.a = 1.0;
	
}