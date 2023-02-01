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

uniform mat4 unVP;
uniform int unMaterialIndex;

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


void main() {
    const int preset = int(step(unLightingSplit, fScreenPos.x));

    vec4 color;
    vec3 normal;
    float metallic;
    float roughness;
    getMaterialPixelInfo(unMaterialIndex, fUV, color, normal, metallic, roughness, fTint);
    color.rgb = gammaDecode(color.rgb, unGamma[preset]); 

    tryDiscardTransparentPixel(color.a);
    
    // Normal to tangent space
    normal = normalize(fTBN * normal);
    
    vec3 sunColor = unSunIntensity * unSunColor;
    oColor.rgb = PBR(fWorldPos, color.rgb, normal, unMetallic, unRoughness, 1.0, sunColor, unLightDir, unCameraPos);
    
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
}