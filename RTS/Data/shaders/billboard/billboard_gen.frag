#include "MaterialData.glsl"
#include "GlobalUbo.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in vec3 fViewTangent;
in vec3 fFragPosTangent;

uniform float unHeightScale = 0.023;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out float oMetallic;
layout (location = 3) out float oRoughness;
layout (location = 4) out float oAO;

uniform uint unDamageTexture = 0;
uniform mat4 unV;

vec3 worldToViewSpaceNormal(vec3 worldNormal) {
    // Transform normal to view space
    vec3 viewNormal = normalize(mat3(unV) * worldNormal);
    return viewNormal;
}

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
    color.a = 1.0;
    
	// Texture tangent space to world space
    vec3 worldNormal = normalize(fTBN * normal);
    // Invert normals if away from camera
    if (!gl_FrontFacing) {
        
        // Doesnt quite work
        //vec3 frontNormal = worldNormal;
       // vec3 backNormal = -worldNormal;
        //float blendFactor = dot(worldNormal, -tangentViewDir);
        //worldNormal = mix(frontNormal, backNormal, blendFactor);
        worldNormal = -worldNormal;
   }
   
    vec3 screenNormal = worldToViewSpaceNormal(worldNormal);
    
    oColor.rgba = color.rgba;
	oNormal.rgb = (screenNormal + 1.0) * 0.5;
    oNormal.a = 1.0;
    oMetallic = metallic;
    oRoughness = roughness;
    oAO = ao;
}