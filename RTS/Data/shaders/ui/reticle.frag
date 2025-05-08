uniform sampler2D unTexture;

in vec2 fUV;

out vec4 fColor;

uniform vec4 unColor = vec4(1.0,1.0,1.0,1.0);

void main() {
    // RG texture
    fColor = texture(unTexture, fUV).rrrg * unColor;
}