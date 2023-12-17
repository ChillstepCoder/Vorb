
vec4 viewPosFromDepth(float depth, vec2 fboUV, mat4 inverseP) {
    vec4 clipSpacePosition = vec4(fboUV * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = inverseP * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition;
}

vec3 worldPosFromDepth(float depth, vec2 fboUV, mat4 inverseV, mat4 inverseP) {

    vec4 worldSpacePosition = inverseV * viewPosFromDepth(depth, fboUV, inverseP);

    return worldSpacePosition.xyz;
}

float linearizeDepth(float d, vec2 cameraZRange) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * cameraZRange.x * cameraZRange.y / (cameraZRange.y + cameraZRange.x - zn * (cameraZRange.y - cameraZRange.x));
}

// Get difference in depth between current pixel and the FBO
float getDepthDiff(sampler2D depthFbo, vec2 fboResolution, vec2 cameraZRange) {
    vec2 fboUV = gl_FragCoord.xy / fboResolution;
    float depth = texture2D(depthFbo, fboUV.xy).r;
    float linFragDepth = linearizeDepth(gl_FragCoord.z, cameraZRange);
    return linearizeDepth(depth, cameraZRange) - linFragDepth;
}