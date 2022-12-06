uniform sampler2D unDiffuse;
uniform sampler2D unNormal;
uniform sampler2D unSpecular;

in vec4 fTint;
in vec2 fUV;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    // Diffuse
    oColor = texture(unDiffuse, fUV.xy).rgba;
    
    // Normal
    vec3 normal = texture(unNormal, fUV.xy).rgb;
	normal = normal * 2.0 - 1.0;
	normal.rgb = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    oNormal.a = 1.0;
    
    // Specular
    oRoughness.rgb = 1.0 - texture(unSpecular, fUV.xy).rgb;
    oRoughness.a = 1.0;
}