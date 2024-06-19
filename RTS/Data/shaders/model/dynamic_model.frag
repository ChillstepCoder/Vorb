#include "MaterialData.glsl"
#include "GlobalUbo.glsl"

#include "util/triplanar.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in vec3 fViewTangent;
in vec3 fFragPosTangent;

uniform float unHeightScale = 0.023;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

uniform uint unDamageTexture = 0;

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
    // Invert normals if away from camera
    if (!gl_FrontFacing) {
        
        // Doesnt quite work
        //vec3 frontNormal = normal;
       // vec3 backNormal = -normal;
        //float blendFactor = dot(normal, -tangentViewDir);
        //normal = mix(frontNormal, backNormal, blendFactor);
        normal = -normal;
   }
    // Into 0-1 range
    
    oColor.rgb = color.rgb;
    
    oColor.a = ao;
   
	oNormal = (normal + 1.0) * 0.5;
    
    oMetallicRoughness.r = metallic;
    oMetallicRoughness.g = roughness;
}