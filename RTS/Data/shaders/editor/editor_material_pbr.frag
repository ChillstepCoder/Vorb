#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "editor/editor_pbr.glsl"
#include "util/tonemapping.glsl"

// Lighting uniforms (MaterialUtils::uploadTonemapUniforms)
uniform vec2 unGamma;
uniform vec2 unExposure;
uniform ivec2 unTonemapOperator;
uniform float unLightingSplit;

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

    vec4 color;
    vec3 normal;
    getMaterialPixelInfo(unMaterialIndex, fUV, color, normal, fTint);

    tryDiscardTransparentPixel(color.a);
    
    // Normal to tangent space
    normal = normalize(fTBN * normal);
    
    oColor.rgb = PBRLearnOpengl(fWorldPos, color.rgb, normal);
    
    // Tonemapping
    const int preset = int(step(unLightingSplit, fScreenPos.x));
    oColor.rgb = computeTonemapping(oColor.rgb, unExposure[preset], unTonemapOperator[preset]);
   
    // Gamma correction
    oColor.rgb = pow(oColor.rgb, vec3(1.0 / unGamma[preset]));
    
   // vec3 color2 = oColor.rgb * 0.0001 + PBRLearnOpengl(fWorldPos, color.rgb, normal);
   // oColor.rgb = color2.rgb  / (oColor.rgb  + vec3(1.0));
   // oColor.rgb  = pow(oColor.rgb , vec3(1.0/2.2));  
    
    oColor.a = 1.0;
    //oColor = getEditorOutputPixelColor(color.rgb, normal, fWorldPos, fScreenPos);
    oNormal = (normal + 1.0) * 0.5;
}