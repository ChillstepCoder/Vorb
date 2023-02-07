#include "MaterialData.glsl"

in vec4 fTint;
in vec2 fUV;
flat in uint fMaterialIndex;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

void main() {
    vec3 normal;
    vec4 color;
    float ao;
    float metallic;
    float roughness;
    getMaterialPixelInfo(fMaterialIndex, fUV, color, normal, ao, metallic, roughness, fTint);
    
    // Albedo
    oColor.rgb = color.rgb;
    // AO
    oColor.a = ao;
    
    // Normal
	normal.rgb = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    
    oMetallicRoughness.r = roughness;
    oMetallicRoughness.g = metallic;
}