#include "../GlobalUbo.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform sampler2D StoneTexture;
uniform sampler2D StoneNormal;
uniform vec3 WaterColor = vec3(0.0 / 255.0, 0.0 / 255.0, 205.0 / 255.0);
uniform vec3 GrassColor = vec3(255.0 / 255.0, 219.0 / 255.0, 105.0 / 255.0);
uniform vec3 StoneColor = vec3(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0);

in float fHeight;
in vec3 fPosition;
in vec2 fUV;
in mat3 fTBN;

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
    
    vec2 farStoneUVs = -(fUV * 0.01);
    vec3 normal;
    
    if (fHeight < 0.0) {
        oColor.rgb = WaterColor;
        normal = vec3(0.0, 0.0, 1.0);
    } else {
        vec3 grassColor = mix(texture(GrassTexture, fUV).rgb, texture(GrassTexture, -(fUV * 0.1)).rgb, 0.4) * GrassColor;
        vec3 stoneColor = mix(texture(StoneTexture, fUV).rgb, texture(StoneTexture, farStoneUVs).rgb, 0.9) * StoneColor;
        
        //stoneColor = stoneColor * 0.00001 + StoneColor;
        float stoneLerp = clamp((fHeight - 6.0) * 0.5, 0.0, 1.0);
        oColor.rgb = mix(grassColor, stoneColor, stoneLerp);
	    normal = mix(vec3(0.0, 0.0, 1.0), texture(StoneNormal, farStoneUVs).xyz * 2.0 - 1.0, stoneLerp);
    }
    
    // === Normals ===
    
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
    
    
    // === Roughness ===
    
	oRoughness.r = 1.0 - texture(GreyNoise, fUV * 4.0).r * 0.4;
	oRoughness.a = 1.0;
}