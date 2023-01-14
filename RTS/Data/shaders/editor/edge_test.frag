uniform sampler2D unNormalFbo;
uniform sampler2D unDepthFbo;
uniform vec2 unCameraZRange;
uniform float unEdgeThreshold;
uniform float unDepthThreshold;

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * unCameraZRange.x * unCameraZRange.y / (unCameraZRange.y + unCameraZRange.x - zn * (unCameraZRange.y - unCameraZRange.x));
}

in vec2 fUV;

layout (location = 0) out vec4 oColor;

void main() {
    vec3 baseNormal = texture(unNormalFbo, fUV).rgb;
    float baseDepth = linearizeDepth(texture(unDepthFbo, fUV).r);
    
    // Edge detect
    // Sobel operator
    // s02 s12 s22
    // s01 s11 s21
    // s00 s10 s20
    // SX
    // -1  0  1
    // -2  0  2
    // -1  0  1
    // SY
    // -1 -2 -1
    //  0  0  0
    //  1  2  1
    ivec2 pix = ivec2(gl_FragCoord.xy);
    vec3 s00 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(-1,-1)).rgb;
    vec3 s10 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(0,-1)).rgb;
    vec3 s20 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(1,-1)).rgb;
    vec3 s01 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(-1,0)).rgb;
    vec3 s21 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(1,0)).rgb;
    vec3 s02 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(-1,1)).rgb;
    vec3 s12 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(0,1)).rgb;
    vec3 s22 = texelFetchOffset(unNormalFbo, pix, 0, ivec2(1,1)).rgb;
    // Depth differences so we dont detect edges with pixels too far away
    float d00 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(-1,-1)).r) - baseDepth);
    float d10 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(0,-1)).r) - baseDepth);
    float d20 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(1,-1)).r) - baseDepth);
    float d01 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(-1,0)).r) - baseDepth);
    float d21 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(1,0)).r) - baseDepth);
    float d02 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(-1,1)).r) - baseDepth);
    float d12 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(0,1)).r) - baseDepth);
    float d22 = abs(linearizeDepth(texelFetchOffset(unDepthFbo, pix, 0, ivec2(1,1)).r) - baseDepth);
    
    vec3 sx = vec3(0.0);
    vec3 sy = vec3(0.0);
    // Ignore edge rows completely since this isnt true edge detect, its intersection detect
    if (dot(s00, s00) != 0 && dot(s20, s20) != 0 && d00 < unDepthThreshold && d20 < unDepthThreshold) {
        sx += s20 - s00;
    }
    if (dot(s01, s01) != 0 && dot(s21, s21) != 0 && d01 < unDepthThreshold && d21 < unDepthThreshold) {
        sx += 2 * (s21 - s01);
    }
    if (dot(s02, s02) != 0 && dot(s22, s22) != 0 && d02 < unDepthThreshold && d22 < unDepthThreshold) {
        sx += s22 - s02;
    }
    if (dot(s00, s00) != 0 && dot(s02, s02) != 0 && d00 < unDepthThreshold && d02 < unDepthThreshold) {
        sy += s00 - s02;
    }
    if (dot(s10, s10) != 0 && dot(s12, s12) != 0 && d10 < unDepthThreshold && d12 < unDepthThreshold) {
        sy += 2 * (s10 - s12);
    }
    if (dot(s20, s20) != 0 && dot(s22, s22) != 0 && d20 < unDepthThreshold && d22 < unDepthThreshold) {
        sy += s20 - s22;
    }
    
    // Standard edge detection
    //vec3 sx = -s00 - vec3(2) * s01 - s02 + s20 + vec3(2) * s21 + s22;
    //vec3 sy = s00 + vec3(2) * s10 + s20 - s02 - vec3(2) * s12 - s22;
    
    vec3 g = sx * sx + sy * sy;
    float mg = max(max(g.x, g.y), g.z);
    if (mg > unEdgeThreshold) {
        oColor.rgb = vec3(1.0, 0.0, 0.0);
    } else {
        oColor.rgb = vec3(0.0, 0.0, 0.0);
    }
    
    oColor.a = 1.0;
    
}