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


vec2 parralaxOffset(float disp, vec3 viewDir) {
    return viewDir.xy / viewDir.z * (disp * unHeightScale);
}

vec2 parallaxMapping(vec2 uvs, float disp, vec3 viewDir) { 
    vec2 p = viewDir.xy / viewDir.z * (disp * unHeightScale);
    return uvs - p;  
}

vec2 superMapping(vec2 uvs, sampler2D disp, vec3 viewDirection) {
    // Variables that control parallax occlusion mapping quality
	const float minLayers = 1.0;
    const float maxLayers = 16.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDirection)));
    numLayers = clamp(numLayers, minLayers, maxLayers);
	float layerDepth = 1.0 / numLayers;
	float currentLayerDepth = 0.0;
	
	// Remove the z division if you want less aberated results
	vec2 S = viewDirection.xy / viewDirection.z * unHeightScale; 
    vec2 deltaUVs = S / numLayers;
    
	
	vec2 UVs = uvs;
	float currentDepthMapValue = 1.0 - texture(disp, UVs).r;
	
	// Loop till the point on the heightmap is "hit"
	while(currentLayerDepth < currentDepthMapValue)
    {
        UVs -= deltaUVs;
        currentDepthMapValue = 1.0 - texture(disp, UVs).r;
        currentLayerDepth += layerDepth;
    }

	// Apply Occlusion (interpolation with prev value)
	vec2 prevTexCoords = UVs + deltaUVs;
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = 1.0 - texture(disp, prevTexCoords).r - currentLayerDepth + layerDepth;
	float weight = afterDepth / (afterDepth - beforeDepth);
	UVs = prevTexCoords * weight + UVs * (1.0 - weight);
    
    return UVs;
}


void main() {
    const int preset = int(step(unLightingSplit, fScreenPos.x));
    
    // Displacement
    vec2 uv = fUV * unUvScale;
    MaterialData mtl = inMaterials[unMaterialIndex];
    if (mtl.displacementMap > 0) {
        vec3 tangentViewDir = normalize(fViewTangent - fFragPosTangent);
        //tangentViewDir.y = -tangentViewDir.y; // IDK I have to or its not correct
        //float disp = sampleMaterialDisplacement(mtl, uv);
        //uv = parallaxMapping(uv, disp, tangentViewDir);
        uv = superMapping(uv, sampler2D(unpackUint2x32(mtl.displacementMap)), tangentViewDir);
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
}