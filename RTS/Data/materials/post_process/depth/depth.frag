uniform sampler2D FboDepth;

in vec2 fUV;

out vec4 fColor;

void main() {

    float z = texture(FboDepth, fUV).r;           // fetch the z-value from our depth texture
    
    fColor.rgb = vec3(z);
	fColor.a = 1.0;
}