uniform sampler2D unFbo;
uniform sampler2D unDepthFbo;
uniform vec2 unCameraZRange;
uniform float unDepthThreshold;

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * unCameraZRange.x * unCameraZRange.y / (unCameraZRange.y + unCameraZRange.x - zn * (unCameraZRange.y - unCameraZRange.x));
}

in vec2 fUV;

layout (location = 0) out vec4 oColor;

void main() {
    float baseVal = texture(unFbo, fUV).r;
    float baseDepth = linearizeDepth(texture(unDepthFbo, fUV).r);
    float depthThreshold = unDepthThreshold;
    
    // Simple cross expandm take max value
    // 0 1 0
    // 1 1 1
    // 0 1 0
    ivec2 pix = ivec2(gl_FragCoord.xy);
    
    float d0 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(-1,0)).r) - baseDepth);
    float d1 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(0,-1)).r) - baseDepth);
    float d2 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(1,0)).r) - baseDepth);
    float d3 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(0,1)).r) - baseDepth);
    float d0valid = float(d0 < unDepthThreshold);
    float d1valid = float(d1 < unDepthThreshold);
    float d2valid = float(d2 < unDepthThreshold);
    float d3valid = float(d3 < unDepthThreshold);
    float s0 = texelFetchOffset(unFbo, pix, 0, ivec2(-1,0)).r * d0valid;
    float s1 = texelFetchOffset(unFbo, pix, 0, ivec2(0,-1)).r * d1valid;
    float s2 = texelFetchOffset(unFbo, pix, 0, ivec2(1,0)).r * d2valid;
    float s3 = texelFetchOffset(unFbo, pix, 0, ivec2(0,1)).r * d3valid;
    
    // We choose a threshold of 1.1 so that we eliminate single pixel edges
    float dTotal = d0valid + d1valid + d2valid + d3valid + 1;
    // SOFT EDGES
    float intensity = (s0 + s1 + s2 + s3 + baseVal) * 1.15 / dTotal;
    oColor.rgb = vec3(min(intensity, 1.0), 0.0, 0.0);
    // HARD EDGES
    //oColor.rgb = vec3(step(1.1, baseVal + s0 + s1 + s2 + s3), 0.0, 0.0);
    oColor.a = 1.0;
    

}