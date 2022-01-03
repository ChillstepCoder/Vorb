uniform sampler2D SSAOTexture;
uniform vec3 SSAOColor;

in vec2 fUV;

out vec4 fColor;

void main() {
    float ssao = texture(SSAOTexture, fUV).r;
	fColor.rgb = SSAOColor;
    fColor.a = 1.0 - ssao;
    //fColor.rgb *= ssao;
}