uniform sampler2DArray Atlas;
uniform sampler2D Fbo0;
uniform sampler2D FboLight;
uniform sampler2D FboDepth;
// uniform sampler2D StarfieldTexture;
uniform vec4 GradientRect;
uniform vec3 CameraFront;
uniform vec3 SunPosition;
uniform float GradientAtlasPage;
uniform float SunHeight;
uniform float Time;
uniform float CameraZAngle;
uniform mat4 VP;

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
	float SunIntensity = max(SunHeight, 0.0);
	float depthHaze = min(pow(depth, 500.0) * (pow(SunIntensity, 0.5) + 0.1 * isGround), 1.0);
	vec2 adjustedUV = fUV;
	adjustedUV.y = SunIntensity;
	vec3 sunTextureColor = texture(Atlas, vec3(GradientRect.xy + adjustedUV * GradientRect.zw, GradientAtlasPage)).rgb;
	fColor.rgb = fColor.rgb * (1.0 - depthHaze * isGround) + vec3(depthHaze) * sunTextureColor;

	// Uncomment for depth render
	//fColor.rgb = fColor.rgb * 0.0001 + depthHaze;
	
	// =====================================================
	// ==                     STARS                       ==
	// =====================================================
	// TODO: These UVs will be squished based on aspect ratio
	//vec2 starsUV = fUV;
	// Add UV based on rotation so the sky rotates (tiling)
	// Zangle is between 0 and 2PI
	//starsUV.x -= CameraZAngle / 1.57079632679489; // PI/2
	//starsUV.y += Time * 0.001;
	//vec4 starsColor = texture(StarfieldTexture, starsUV).rgba;
	//float sparkle = (sin(Time * 1.25 + starsUV.y * 700.0) + 1.3) * 0.434782;
	//float starIntensity = pow(1.0 - SunIntensity, 20.0);
	//fColor.rgb += starsColor.rgb * starsColor.a * isSky * sparkle * starIntensity;

	// =====================================================
	// ==                     SUN                         ==
	// =====================================================
	// Sun Glow
	vec3 screenPos = normalize(vec3(1.0, fUV.x * 2.0 - 1.0, fUV.y * 2.0 - 1.0));
	float sunAngle = max(pow(dot(SunPosition, screenPos), 64.0), 0.0);
	fColor.rgb += sunAngle * max(pow(SunIntensity, 0.5) - 0.1, 0.0);
	// Sky sun glow + sun texture
	fColor.rgb += isSky * (sunAngle * 0.5 + max(pow(sunAngle - 0.95, 0.3), 0.0) * 2.0);
	fColor.a = 1.0;
}