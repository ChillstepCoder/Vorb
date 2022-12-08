#include "MaterialData.glsl"

in vec4 fTint;
in vec2 fUV;
flat in uint fMaterialIndex;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
    // Diffuse
    vec4 color = mtl.albedoColor;
	vec3 normal = vec3(0.0, 0.0, 1.0);
    
    if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, fUV);
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, fUV);
        normal = normal * 2.0 - 1.0;
    }
    
    // Diffuse
    oColor = color;
    
    // Normal
	normal.rgb = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    oNormal.a = 1.0;
    
    // Specular (OLD)
    oRoughness.rgb = vec3(0.5);
    oRoughness.a = 1.0;
}