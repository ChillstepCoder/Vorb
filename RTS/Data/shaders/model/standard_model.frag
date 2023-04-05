#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in vec3 fViewTangent;
in vec3 fFragPosTangent;

uniform float unHeightScale = 1.0;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

void main() {

    // Displacement
    vec2 uv = fUV;
    MaterialData mtl = inMaterials[fMaterialIndex];
    if (mtl.displacementMap > 0) {
        vec3 tangentViewDir = normalize(fViewTangent - fFragPosTangent);
        uv = dispMapping(uv, sampler2D(unpackUint2x32(mtl.displacementMap)), tangentViewDir, unHeightScale);
    }  


    vec3 normal;
    vec4 color;
    float ao;
    float metallic;
    float roughness;
    getMaterialPixelInfo(fMaterialIndex, uv, color, normal, ao, metallic, roughness, fTint);

    tryDiscardTransparentPixel(color.a);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    // Into 0-1 range
	oNormal = (normal + 1.0) * 0.5;
    
    oColor.rgb = color.rgb;
    oColor.a = ao;
    
    oMetallicRoughness.r = metallic;
    oMetallicRoughness.g = roughness;
}