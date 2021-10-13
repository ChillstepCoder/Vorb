uniform sampler2DArray Atlas;
uniform sampler2D Fbo0;
uniform sampler2D FboLight;
uniform sampler2D FboDepth;
uniform vec4 GradientRect;
uniform vec3 SunPosition;
uniform float GradientAtlasPage;
uniform float SunHeight;
uniform float Time;
uniform mat4 InverseVP;


in vec2 fUV;

out vec4 fColor;

void main() {

	float depth = texture(FboDepth, fUV).r;
	float isSky = step(0.999999999, depth);
	float isGround = 1.0 - isSky;
	
    fColor.rgb = texture(FboLight, fUV).rgb * texture(Fbo0, fUV).rgb;
	
	// =====================================================
	// ==                     HAZE                        ==
	// =====================================================
	float sunIntensity = max(SunHeight, 0.0);
	float adjustedDepth = pow(depth, 500.0);
	float depthHaze = min(adjustedDepth * (pow(sunIntensity, 0.5) + 0.1 * isGround), 1.0);
	vec2 adjustedUV = fUV;
	adjustedUV.y = sunIntensity;
	vec3 sunTextureColor = texture(Atlas, vec3(GradientRect.xy + adjustedUV * GradientRect.zw, GradientAtlasPage)).rgb;
	// Day Haze
	fColor.rgb = fColor.rgb * (1.0 - depthHaze * isGround) + depthHaze * sunTextureColor;

	// Night Haze
	float nightHaze = 1.0 - adjustedDepth * isGround * (1.0 - pow(sunIntensity, 0.2));
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
	fColor.rgb += sunAngle * max(pow(sunIntensity, 0.5) - 0.1, 0.0);
	// Sky sun glow + sun texture
	fColor.rgb += isSky * (sunAngle * 0.5 + max(pow(sunAngle - 0.95, 0.3), 0.0) * 2.0);
	fColor.a = 1.0;
	
	
	// Uncomment for depth render
}