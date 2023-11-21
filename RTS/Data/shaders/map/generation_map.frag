
in vec2 fUV;

out vec4 fColor;

const vec3 colors[4] = {
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 0.0)
}
const vec2 cornerUVs[4] = {
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
}

void main() {
    vec3 color = vec4(0.0, 0.0, 0.0);
    for (int i = 0; i < 4; ++i) {
        
    }
    color = color / 4;
    fColor = vec4(color.r, color.g, color.b, 1.0);
}