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
uniform float unHeightScale = 1.0;

uniform mat4 unVP;
uniform int unMaterialIndex;
uniform vec2 unUvScale = vec2(2.0);
uniform bool unOverrideMR;

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
in vec4 fTint;
in mat3 fTBN;
in vec3 fViewTangent;
in vec3 fFragPosTangent;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


void main() {
    const int preset = int(step(unLightingSplit, fScreenPos.x));
    
    // Displacement
    vec2 uv = fUV * unUvScale;
    MaterialData mtl = inMaterials[unMaterialIndex];
    if (mtl.displacementMap > 0) {
        vec3 tangentViewDir = normalize(fViewTangent - fFragPosTangent);
        uv = dispMapping(uv, sampler2D(unpackUint2x32(mtl.displacementMap)), tangentViewDir, unHeightScale);
    }  

    vec4 color;
    vec3 normal;
    float ao;
    float metallic;
    float roughness;
    getMaterialPixelInfo(unMaterialIndex, uv, color, normal, ao, metallic, roughness, fTint);
    color.rgb = gammaDecode(color.rgb, unGamma[preset]); 
    
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
    
   // vec3 color2 = oColor.rgb * 0.0001 + PBRLearnOpengl(fWorldPos, color.rgb, normal);
   // oColor.rgb = color2.rgb  / (oColor.rgb  + vec3(1.0));
   // oColor.rgb  = pow(oColor.rgb , vec3(1.0/2.2));  
   
    //oColor.rgb = oColor.rgb * 0.00001 + PBR(fWorldPos, color.rgb, normal);
    //oColor.rgb = pow(oColor.rgb, vec3(1.0 / 2.2));
    
    oColor.a = 1.0;
    //oColor = getEditorOutputPixelColor(color.rgb, normal, fWorldPos, fScreenPos);
    oNormal = (normal + 1.0) * 0.5;
    
    // Debug displacement
   // if (mtl.displacementMap > 0) {
       // vec3 tangentViewDir = normalize(fViewTangent - fFragPosTangent);
        //float disp = sampleMaterialDisplacement(mtl, uv);
        // oColor.rgb = oNormal;
        //oColor.r = disp;
        //oColor.rgb = tangentViewDir;
        //oColor.rgb = (fTBN * vec3(0.0, 0.0, 1.0) + 1.0) * 0.5;
        //oColor.rg = parallaxMapping(uv, disp, tangentViewDir);
        //oColor.rg = parralaxOffset(disp, tangentViewDir);
   // }
    //oColor.rg = 0.0001 * oColor.rg + fUV;
    
    
    
    //oColor.rgb = 0.0001 * oColor.rgb + oNormal.rgb; //fViewTangent fFragPosTangent
    //oColor.rgb = 0.0001 * oColor.rgb + fFragPosTangent.rgb; //fViewTangent fFragPosTangent
}