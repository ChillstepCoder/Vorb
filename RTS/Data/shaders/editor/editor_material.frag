#include "MaterialData.glsl"

in vec2 fUV;
in vec4 fTint;
in mat3 fTBN;

// 0 = default
// 1 = normals
// 2 = uvs
uniform int unRenderMode = 0;
uniform int unMaterialIndex;

layout (location = 0) out vec4 oColor;

void main() {
    MaterialData mtl = inMaterials[unMaterialIndex];
    
    vec4 color = mtl.albedoColor;
	vec3 normal = vec3(0.0, 0.0, 1.0);

	if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, fUV) * fTint;
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, fUV);
        normal = normal * 2.0 - 1.0;
    }

    // Don't write 0 alpha
    if (color.a < 0.01) {
        discard;
    }
    color.a = 1.0;
    
    normal = normalize(fTBN * normal);
    
    // Lighting
    
    // Color
    if (unRenderMode == 1) {
        // Normals render
        oColor.rgb = (normal + 1.0) * 0.5;
        oColor.a = color.a;
    } else if (unRenderMode == 2) {
        // UVs render
        oColor.rg = fUV;
        oColor.b = 0.0;
        oColor.a = 1.0;
    } else {
        oColor = color;
    }
}