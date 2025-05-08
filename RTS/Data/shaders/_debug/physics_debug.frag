// Uniforms
uniform vec3 unLightDir = vec3(-1.0, -1.0, -1.0);

// Input from vertex shader
in vec4 fColor;
in vec3 fNormal;
in vec3 fWorldPos;
uniform float unAlpha = 1.0;

// Output
out vec4 pColor;

void main() {
    vec3 normal = normalize(fNormal);
    vec3 lightDir = normalize(-unLightDir);
    
    // Ambient light
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * fColor.rgb;
    
    // Diffuse light
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * fColor.rgb;
    
    // Combine lighting
    vec3 result = ambient + diffuse;
    pColor = vec4(result, fColor.a * unAlpha);
}