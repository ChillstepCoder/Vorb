
#include "TextureUbo.glsl"

in vec2 fUV;
flat in int fTextureIndex;
in mat3 fTBN;

layout (location=0) out vec4 out_FragColor;


void main()
{
    vec4 diffuseColor = texture(sampler2D(Textures[fTextureIndex].xy), fUV);
    if (diffuseColor.a < 0.01) {
        discard;
    }
    
    vec3 normal = texture(sampler2D(Textures[fTextureIndex].zw), fUV).rgb;
	normal = normal * 2.0 - 1.0;
	normal = normalize(fTBN * normal);
	out_FragColor.rgb = (normal + 1.0) * 0.5;
    out_FragColor.a = 1.0;

};
