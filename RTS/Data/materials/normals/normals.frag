uniform sampler2DArray Atlas;
uniform float Time;

in vec2 fUV;

out vec4 fColor;

void main() {
    //fColor = texture(Atlas, vec3(fUV, 0)) * vec4(1.0, (sin(Time) + 1.0) * 0.5, 0.5, 0.8);
	vec2 offset = fUV - vec2(0.5, 0.5);
	float dist = length(offset);
	fColor.xyz = vec3((sin(dist * 200 + Time * 20) + 1.0) * 0.5);
	//fColor.xyz = vec3(dist * 2);
	fColor.a = 0.02;
}