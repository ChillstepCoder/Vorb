uniform sampler2D FboRoughness;

in vec2 fUV;

out vec4 fColor;

void main() {

    vec2 rm = texture(FboRoughness, fUV).xy;           // fetch the z-value from our depth texture
    
    fColor.rg = rm;
    fColor.b = 0.0;
	fColor.a = 1.0;
}