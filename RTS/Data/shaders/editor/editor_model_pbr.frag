#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "editor/editor_pbr.glsl"
#include "util/gamma.glsl"
#include "util/tonemapping.glsl"

// Lighting uniforms (MaterialUtils::uploadTonemapUniforms)
uniform vec2 unGamma;
uniform vec2 unExposure;
uniform ivec2 unTonemapOperator;
uniform float unLightingSplit;

uniform float unSunIntensity;
uniform vec3 unSunColor;
uniform float unMetallic;
uniform float unRoughness;
uniform vec3 unLightDir;
uniform vec3 unCameraPos;
uniform bool unOverrideMR;
uniform float unHeightScale = 1.0;

uniform mat4 unVP;

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in vec3 fViewTangent;
in vec3 fFragPosTangent;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;

void main() {
    const int preset = int(step(unLightingSplit, fScreenPos.x));

    vec3 normal;
    vec4 color;
    float ao;
    float metallic;
    float roughness;
    
    MaterialData mtl = inMaterials[fMaterialIndex];
    vec2 uv = fUV;
    if (mtl.displacementMap > 0) {
        vec3 tangentViewDir = normalize(fViewTangent - fFragPosTangent);
        uv = dispMapping(uv, sampler2D(unpackUint2x32(mtl.displacementMap)), tangentViewDir, unHeightScale);
    }
    
    getMaterialPixelInfo(fMaterialIndex, uv, color, normal, ao, metallic, roughness, fTint);
    color.rgb = gammaDecode(color.rgb, unGamma[preset]);
    //normal.rgb = gammaDecode((normal.rgb + 1.0) * 0.5, unGamma[preset]) * 2.0 - 1.0;
    
    if (unOverrideMR) {
        metallic = unMetallic;
        roughness = unRoughness;
    }
    
    tryDiscardTransparentPixel(color.a);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    
    vec3 sunColor = unSunIntensity * unSunColor;
    oColor.rgb = PBR(fWorldPos, color.rgb, normal, metallic, roughness, ao, sunColor, unLightDir, unCameraPos);
    
    // Tonemapping
    oColor.rgb = computeTonemapping(oColor.rgb, unExposure[preset], unTonemapOperator[preset]);
   
    // Gamma correction
    oColor.rgb = gammaCorrection(oColor.rgb, unGamma[preset]);
    
    oColor.a = 1.0;
    oNormal = (normal + 1.0) * 0.5;
    
    //oColor.rgb = 0.0001 * oColor.rgb + fViewTangent.rgb; //fViewTangent fFragPosTangent
}