#include "MaterialData.glsl"

in vec4 fTint;
in vec2 fUV;
flat in uint fMaterialIndex;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out float oRoughness;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
    // Diffuse
    vec3 color = mtl.albedoColor.rgb;
	vec3 normal = vec3(0.0, 0.0, 1.0);
    
    if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, fUV).rgb;
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, fUV);
        normal = normal * 2.0 - 1.0;
    }
    
    // Albedo
    oColor.rgb = color;
    // Metallic
    oColor.a = getMaterialMetallic(mtl);
    
    // Normal
	normal.rgb = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    
    oRoughness.r = getMaterialRoughness(mtl);
}