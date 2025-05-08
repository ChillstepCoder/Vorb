uniform sampler2D unThresholdTexture;
uniform sampler2D unSpawnTexture;

uniform int unShowThreshold;
uniform int unShowSpawns;

in vec2 fUV;

out vec4 fColor;

void main() {
    vec3 pixelColor = vec3(0.0);
    if (unShowThreshold == 1) {
        pixelColor = texture(unThresholdTexture, fUV).rrr;
    }
    if (unShowSpawns == 1) {
        pixelColor = mix(pixelColor, vec3(1.0, 0.0, 0.0), texture(unSpawnTexture, fUV).r * 0.75);
    }
    
    fColor.rgb = pixelColor;
    fColor.a = 1.0;
}