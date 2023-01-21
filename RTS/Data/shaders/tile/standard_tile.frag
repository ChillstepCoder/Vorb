#include "TextureUbo.glsl"
uniform sampler2DArray Atlas;

in vec2 fUV;
flat in int fTextureIndex;
in vec4 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec3 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec3 oRoughness;

void main() {
    vec4 color = texture(sampler2D(Textures[fTextureIndex].xy), fUV) * fTint;
	
    // TODO: AlphaTest
    if (color.a < 0.85) {
        discard;
    }
	oColor = color.rgb;
	
	// Normal is always the next page
	vec3 normal = texture(sampler2D(Textures[fTextureIndex].zw), fUV).rgb;
	normal = normal * 2.0 - 1.0;
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oRoughness.r = fRoughness;
    
}