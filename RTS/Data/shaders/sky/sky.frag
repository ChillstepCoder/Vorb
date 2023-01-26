// TODO: Indoor mask
uniform sampler2D StarfieldTexture;
uniform samplerCube unSkyboxCube;

#include "../GlobalUbo.glsl"

in vec2 fUV;
in vec3 fPosition;
in vec3 fSkyVector;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


void main() {
	// Add UV based on rotation so the sky rotates (tiling)
	// Zangle is between 0 and 2PI
	float SunIntensity = max(SunHeight, 0.0);
	vec4 starsColor = texture(StarfieldTexture, fUV).rgba;
	float positionInput = (fPosition.z - fPosition.x - fPosition.y) * 700.0;
	float sparkle = (sin(Time * 1.25 + positionInput) + 1.3) * 0.434782;
	float starIntensity = pow(1.0 - SunIntensity, 20.0);
    vec3 color = starsColor.rgb * starsColor.a * sparkle * starIntensity;
    
    // Clouds
    
    // TODO: Fix stars
    color.rgb = color.rgb * 0.00001;

    // NEW CUBEMAP
    vec3 color2 = texture(unSkyboxCube, fSkyVector).rgb;
    
    // TODO: Remove old
    oColor.rgb = color.rgb + color2;
    oColor.a = 1.0;
    
    oNormal.rgb = vec3(0.0);
}