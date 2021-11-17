uniform sampler2DArray ShadowMap;

in vec2 fUV;

out vec4 fColor;

void main() {

    float z = texture(ShadowMap, vec3(fUV, 0)).r;           // fetch the z-value from our depth texture
    
    fColor.rgb = vec3(z);
	fColor.a = 1.0;
}