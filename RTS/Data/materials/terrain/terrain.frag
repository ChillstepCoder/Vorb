#include "../GlobalUbo.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform sampler2D StoneTexture;
uniform sampler2D StoneNormal;
uniform vec3 WaterColor = vec3(0.0 / 255.0, 100.0 / 255.0, 155.0 / 255.0);
uniform vec3 GrassColor = vec3(255.0 / 255.0, 219.0 / 255.0, 105.0 / 255.0);
uniform vec3 StoneColor = vec3(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0);

uniform float unHeightMult = 0.191;
uniform float unWavyMult = 0.167;
uniform float unSquaresIntensity = 0.5;
uniform float unSquaresPeriod = 0.187;
uniform float unBlendMult = 0.037;

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

const int NUM_COLORS = 12;

const vec3 COLORS[NUM_COLORS] = {
    vec3(128.0 / 255.0, 95.0 / 255.0, 106.0 / 255.0),
    vec3(113.0 / 255.0, 79.0 / 255.0, 96.0 / 255.0),
    vec3(188.0 / 255.0, 165.0 / 255.0, 131.0 / 255.0),
    vec3(74.0 / 255.0, 157.0 / 255.0, 139.0 / 255.0),
    vec3(155.0 / 255.0, 141.0 / 255.0, 138.0 / 255.0),
    vec3(151.0 / 255.0, 133.0 / 255.0, 121.0 / 255.0),
    vec3(146.0 / 255.0, 167.0 / 255.0, 152.0 / 255.0),
    vec3(130.0 / 255.0, 147.0 / 255.0, 155.0 / 255.0),
    vec3(66.0 / 255.0, 63.0 / 255.0, 56.0 / 255.0),
    vec3(130.0 / 255.0, 145.0 / 255.0, 126.0 / 255.0),
    vec3(160.0 / 255.0, 169.0 / 255.0, 152.0 / 255.0),
    vec3(86.0 / 255.0, 81.0 / 255.0, 75.0 / 255.0),
};

float triangularWave(float val) {
    val = mod(val, 4.0);
    if (val <= 1.0) {
        return val;
    } else if (val <= 3.0) {
        return 2.0 - val;
    } else {
        return val - 4.0;
    }
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
    vec2 farGrassUVs = -(fUV * 0.1);
    
    vec3 normal;
    float distance = length(fPosition.xy);
    float distUvLerp = min(distance * 0.001, 1.0);
    float lerpNoise = -texture(GreyNoise, fUV * 0.05).r * 10.0;
    float stoneLerpHeight = fHeight - fTBN[2].z * 20.0 + lerpNoise; // Include surface normal val
    
    if (fHeight < 0.0) {
        oColor.rgb = WaterColor;
        normal = vec3(0.0, 0.0, 1.0);
    } else {
        vec3 grassColor = mix(texture(GrassTexture, fUV).rgb, texture(GrassTexture, farGrassUVs).rgb, distUvLerp) * GrassColor;
        vec3 stoneColor = mix(texture(StoneTexture, fUV).rgb, texture(StoneTexture, farStoneUVs).rgb, distUvLerp) * StoneColor;
        
        //stoneColor = stoneColor * 0.00001 + StoneColor;
        float stoneLerp = clamp((stoneLerpHeight - 6.0) * 0.5, 0.0, 1.0);
        oColor.rgb = mix(grassColor, stoneColor, stoneLerp);
	    normal = mix(vec3(0.0, 0.0, 1.0), texture(StoneNormal, farStoneUVs).xyz * 2.0 - 1.0, stoneLerp);
    }
    
    // === Normals ===
    // TODO: RESTORE NORMAL MAPPING
    normal = normal * 0.00001 + vec3(0.0, 0.0, 1.0);
    
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
    
    // Debug distance lerp
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(distUvLerp, 0.0, 0.0);
    
    // =========== BEGIN NEW ART STYLE ==========

    oColor.rgb = oColor.rgb * 0.00001;
    
    vec3 colorNormal = normal.rgb; // normal
    
    float FLAT_REDUCE_MULT = 1.0;
    float GRANULARITY_MULT = unWavyMult;
    float GRANULARITY = 12.0 * GRANULARITY_MULT * (1.0 - colorNormal.z * FLAT_REDUCE_MULT);
    float UV_MOD_MULT = unSquaresPeriod;
    float uvMod = 0.5 + (triangularWave(fUV.x * UV_MOD_MULT) + triangularWave(fUV.y * UV_MOD_MULT)) * unSquaresIntensity * 2.0;
    float HEIGHT_ADD = (fHeight * uvMod) * 0.024 * unHeightMult * 2.0;
    colorNormal = vec3(abs(colorNormal.x) * GRANULARITY + HEIGHT_ADD, abs(colorNormal.y) * GRANULARITY + HEIGHT_ADD, colorNormal.z * GRANULARITY);
    
    float totalNormal = colorNormal.x + colorNormal.y;
    
    float lowIndex = round(totalNormal);
    //float lowIndex = floor(totalNormal);
    float highIndex = ceil(totalNormal);
    //int index = int(round(totalNormal));
    int index = int(lowIndex + 5) % NUM_COLORS;
    int index2 = int(highIndex + 5) % NUM_COLORS;
    
    
    float lerpVal = mod(totalNormal, 1.0);
    // Shorten the transition
    lerpVal = (lerpVal - 0.3) * 12.0 * unBlendMult;
    lerpVal = clamp(lerpVal, 0.0, 1.0);
    oColor.rgb = oColor.rgb + mix(COLORS[index], COLORS[index2], lerpVal); // PRETTY RAINBOW + 0.5 * vec3((cos(fUV.x * 0.1) + 1.0) * 0.5, (cos(fUV.y * 0.1) + 1.0) * 0.5, (cos(fUV.y * 0.1 - fUV.x * 0.1) + 1.0) * 0.5);
    //oColor.rgb = oColor.rgb + COLORS[index];
    //oColor.rgb = oColor.rgb + COLORS[index] * texture(GrassTexture, fUV).rgb;
    //oColor.rgb *= texture(GrassTexture, fUV).rgb;
    
    // =========== END NEW ART STYLE ==========
    
    // === Roughness ===
    
	oRoughness.r = 1.0 - texture(GreyNoise, fUV * 4.0).r * 0.4;
	oRoughness.a = 1.0;
}