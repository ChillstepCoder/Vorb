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
#include "../../GlobalUbo.glsl"


#include "../../util/lighting.glsl"

in vec2 fUV;

out vec4 fColor;

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
    fColor.rgb = (isGround * lightTotal * SunColor + isSky) * fboColor;
	
	// =====================================================
	// ==                     HAZE                        ==
	// =====================================================
	float adjustedDepth = pow(depth, 500.0);
	float depthHaze = min(adjustedDepth * (pow(hazeIntensity, 0.5) + 0.1 * isGround), 1.0);
	vec2 adjustedUV = fUV;
	adjustedUV.y = hazeIntensity;
	vec3 sunTextureColor = texture(Atlas, vec3(GradientRect.xy + adjustedUV * GradientRect.zw, GradientAtlasPage)).rgb;
	// Day Haze
	fColor.rgb = fColor.rgb * (1.0 - depthHaze * isGround) + depthHaze * sunTextureColor;

	// Night Haze
	float nightHaze = 1.0 - adjustedDepth * isGround * (1.0 - pow(hazeIntensity, 0.2));
	fColor.rgb *= nightHaze;
	
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
	fColor.rgb = computePhong(fColor.rgb, normal, SunPosition, max(isSky, 0.5), roughness, depth, fUV, shadow);
	
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
	
	fColor.a = 1.0;
	
}