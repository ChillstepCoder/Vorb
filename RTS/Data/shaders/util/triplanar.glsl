
// For luma blend
float getLuminance(vec3 color) {
    return (0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b);
}

// https://bgolus.medium.com/normal-mapping-for-a-triplanar-shader-10bf39dca05a#38e5
// Returns weights for X,Y,Z respectively
vec3 computeTriPlanarBlend(vec3 norm, float lumaX, float lumaY, float noiseBlend, float strength, float blendHardness) {
	// Asymmetric Triplanar Blend
    vec3 blend = vec3(0.0); // Blend for sides only
    vec2 xyBlend = normalize(abs(norm.xy));
    blend.xy = max(vec2(0.0), xyBlend - vec2(0.67));
    blend.xy /= max(0.00001, dot(blend.xy, vec2(1,1)));// Blend for top
    
    // Luminance of cliff affects blend
    noiseBlend += max(lumaX * blend.x, lumaY * blend.y) * 15.0;
    blend.z = clamp((abs(norm.z) - strength) * blendHardness + noiseBlend, 0.0, 1.0);
    blend.xy *= (1.0 - blend.z);
    return blend;
}

vec3 computeTriplanarNormal(vec3 surfaceNorm, sampler2D textureNorm, vec2 uvX, vec2 uvY, vec3 blend) {
    // Whiteout blend

    // Tangent space normal maps
    vec3 tnormalX = texture(textureNorm, uvX).rgb * 2.0 - vec3(1.0);
    vec3 tnormalY = texture(textureNorm, uvY).rgb * 2.0 - vec3(1.0);
    vec3 tnormalZ = vec3(0.0, 0.0, 1.0);

    // Swizzle world normals into tangent space and apply Whiteout blend
    tnormalX = vec3(
        tnormalX.xy + surfaceNorm.zy,
        abs(tnormalX.z) * surfaceNorm.x
    );
    tnormalY = vec3(
        tnormalY.xy + surfaceNorm.xz,
        abs(tnormalY.z) * surfaceNorm.y
    );
    tnormalZ = vec3(
        tnormalZ.xy + surfaceNorm.xy,
        abs(tnormalZ.z) * surfaceNorm.z
    );

    // Swizzle tangent normals to match world orientation and triblend
    vec3 worldNormal = normalize(
        tnormalX.zyx * blend.x +
        tnormalY.xzy * blend.y +
        tnormalZ.xyz * blend.z
    );
    
    return worldNormal.xyz;
}


vec3 computeTriplanarNormal(vec3 surfaceNorm, vec3 normals[3], vec3 blend) {
    // Whiteout blend

    // Tangent space normal maps
    vec3 tnormalX = normals[0];
    vec3 tnormalY = normals[1];
    vec3 tnormalZ = normals[2];

    // Swizzle world normals into tangent space and apply Whiteout blend
    tnormalX = vec3(
        tnormalX.xy + surfaceNorm.zy,
        abs(tnormalX.z) * surfaceNorm.x
    );
    tnormalY = vec3(
        tnormalY.xy + surfaceNorm.xz,
        abs(tnormalY.z) * surfaceNorm.y
    );
    tnormalZ = vec3(
        tnormalZ.xy + surfaceNorm.xy,
        abs(tnormalZ.z) * surfaceNorm.z
    );

    // Swizzle tangent normals to match world orientation and triblend
    vec3 worldNormal = normalize(
        tnormalX.zyx * blend.x +
        tnormalY.xzy * blend.y +
        tnormalZ.xyz * blend.z
    );
    
    return worldNormal.xyz;
}
