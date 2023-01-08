#include "MaterialData.glsl"

in vec2 fUV;
in vec4 fTint;
in mat3 fTBN;

uniform int unRenderNormals = 0;
uniform int unMaterialIndex;

layout (location = 0) out vec4 oColor;

void main() {
    MaterialData mtl = inMaterials[unMaterialIndex];
    
    vec4 color = mtl.albedoColor;
	vec3 normal = vec3(0.0, 0.0, 1.0);

	if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, fUV);
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, fUV);
        normal = normal * 2.0 - 1.0;
    }
    oColor.a = 1.0;

    // Don't write 0 alpha
    if (oColor.a < 0.01) {
        discard;
    }
    
    normal = normalize(fTBN * normal);
    
    // Lighting
    
    // Color
    if (unRenderNormals != 0) {
        oColor.rgb = (normal + 1.0) * 0.5;
    } else {
        oColor = color * fTint;
    }
}