#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
    
    vec4 color = mtl.albedoColor;
	vec3 normal = vec3(0.0, 0.0, 1.0);

	if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, fUV);
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, fUV);
        normal = normal * 2.0 - 1.0;
    }

    oColor = color * fTint;
    // Don't write 0 alpha
	
    if (oColor.a < 0.01) {
        discard;
    }
	
	// Normal is always the next page
    normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oRoughness.r = 0.2;
	oRoughness.a = 1.0;
    
    oColor.a = 1.0;
	oNormal.a = oColor.a;
}