#include "../GlobalUbo.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform vec3 WaterColor = vec3(0.0 / 255.0, 0.0 / 255.0, 205.0 / 255.0);
uniform vec3 GrassColor = vec3(255.0 / 255.0, 219.0 / 255.0, 105.0 / 255.0);

in float fHeight;
in vec3 fPosition;
in vec2 fUV;
in mat3 fTBN;
in float fRoughness;

uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)

layout (location = 0) out vec4 oColor; // TODO: vec3
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

float InvSmoothStep(float x) {
    return x + (x - (x * x * (3.0 - 2.0 * x)));
}

void main() {
	
    // === Crossfade ===
    
    // TODO: Render crossfading with separate material
    vec4 screenPos = (VP * vec4(fPosition, 1.0));
    vec2 screenUV = (screenPos.xy / vec2(screenPos.w));
	float noiseVal = texture(GreyNoise, screenUV * 3.0).r;
    float alpha = unCrossfadeAlpha * 0.05 + noiseVal * unCrossfadeAlpha + 0.4;
	alpha = InvSmoothStep(alpha);
	oColor.a = min(mix(1.0 - alpha, alpha, unCrossfadeDirection), 1.0);
	oColor.a = clamp(oColor.a, 0.0, 1.0);
	
	if (oColor.a <= 0.5) {
        discard;
    }
    oColor.a = 1.0;
    
    // === Terrain texturing ===
    
    if (fHeight < -2.25) {
        oColor.rgb = WaterColor;
    } else {
        oColor.rgb = (texture(GrassTexture, fUV).rgb + texture(GrassTexture, -(fUV * 0.1)).rgb) * 0.5;
        oColor.rgb = oColor.rgb * GrassColor;
    }
    
    // === Normals ===
    
	vec3 normal = vec3(0.0, 0.0, 1.0);
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
    
    
    // === Roughness ===
    
	oRoughness.r = fRoughness;
	oRoughness.a = 1.0;
}