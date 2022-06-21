#include "TextSSBO.glsl"

in vec2 fUV;
flat in int fTextureIndex;
in vec4 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    int arrayIndex = fTextureIndex / 2;
    int vecOffset = (fTextureIndex % 2) * 2;
    // We are necoding two textures into a sincle uvec4 for efficiency
    uvec4 txSet = fontTextures[arrayIndex];
    oColor = texture(sampler2D(uvec2(txSet[vecOffset], txSet[vecOffset + 1])), fUV) * fTint;
    // Don't write 0 alpha (TMP?)
    if (oColor.a < 0.40) {
        discard;
    }
	oColor.a = 1.0;
	oNormal = vec4(0.0, 0.0, 1.0, 1.0);
    oRoughness = vec4(fRoughness, fRoughness, fRoughness, 1.0);
	
}