#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "editor/editor_pbr.glsl"
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

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;

void main() {

    vec3 normal;
    vec4 color;
    float metallic;
    float roughness;
    getMaterialPixelInfo(fMaterialIndex, fUV, color, normal, metallic, roughness, fTint);
    
    tryDiscardTransparentPixel(color.a);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    
    vec3 sunColor = unSunIntensity * unSunColor;
    oColor.rgb = PBR(fWorldPos, color.rgb, normal, unMetallic, unRoughness, 1.0, sunColor, unLightDir, unCameraPos);
    
    // Tonemapping
    const int preset = int(step(unLightingSplit, fScreenPos.x));
    oColor.rgb = computeTonemapping(oColor.rgb, unExposure[preset], unTonemapOperator[preset]);
   
    // Gamma correction
    oColor.rgb = pow(oColor.rgb, vec3(1.0 / unGamma[preset]));
    
    oColor.a = 1.0;
    //oColor = getEditorOutputPixelColor(color.rgb, normal, fWorldPos, fScreenPos);
    oNormal = (normal + 1.0) * 0.5;
}