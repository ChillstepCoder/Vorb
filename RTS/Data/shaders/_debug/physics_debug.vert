// Uniforms
uniform mat4 unVP;
uniform vec3 unCameraPos;

// Input
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 3) in vec4 vColor;
layout(location = 4) in mat4 vModelMatrix;
layout(location = 8) in vec4 vModelColor;

// Output to fragment shader
out vec4 fColor;
out vec3 fNormal;
out vec3 fWorldPos;

void main() {
    vec4 worldPos = vModelMatrix * vPosition;
    fWorldPos = worldPos.xyz;
    fNormal = mat3(vModelMatrix) * vNormal;  // Transform normal to world space
    fColor = vColor * vModelColor;
    
    worldPos.xyz -= unCameraPos;
    gl_Position = unVP * worldPos;
}