
#include "GlobalUbo.glsl"
#include "GrassUbo.glsl"
#include "NormalUtil.glsl"

uniform samplerBuffer UnTboPosition;
uniform samplerBuffer UnTboSizeType;
uniform samplerBuffer UnTboNormal;
uniform vec3 unPosition;
uniform float UnYOffset = 1.0;
uniform vec2 unScale;
uniform float unLeanVariance;

out vec3 fWorldPos;
flat out vec3 fWorldRoot;
flat out vec2 fRelXY;
out float fHeight;
out vec2 fUV;
flat out int fGrassMaterial;
out float fDistance;
out vec2 fLean;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

// TODO: Match number of grass types
const float GRASS_UV_X = 1.0;
const float GRASS_UV_Y = 1.0; // Single row rn
const vec2 UVS[8] = {
 // NORMAL UVS
 {0.0, 0.0 },
 {GRASS_UV_X, 0.0 },
 {GRASS_UV_X, GRASS_UV_Y },
 {0.0, GRASS_UV_Y },
 // INVERT UVS AFTER HERE
 {GRASS_UV_X, 0.0 },
 {0.0, 0.0 },
 {0.0, GRASS_UV_Y },
 {GRASS_UV_X, GRASS_UV_Y }
};

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[gl_VertexID % 4];
	vertexOffsets.y += UnYOffset;
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

// Fast pesudorandom
float rand(vec2 co){
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    int bladeIndex = (gl_VertexID / 4);
	vec4 vPosition = vec4(texelFetch(UnTboPosition, bladeIndex).rgb, 1.0);
	vec4 dimsTypeRotation = texelFetch(UnTboSizeType, bladeIndex);
    vec2 normal2 = (texelFetch(UnTboNormal, bladeIndex).rg * 2.0) - 1.0;
    vec3 normal = reconstructNormal(normal2);
    
    int grassID = int(round(dimsTypeRotation.z * 255.0));
	vec2 vDims = dimsTypeRotation.xy * vec2(unGrassData[grassID].grassScaleX, unGrassData[grassID].grassScaleY) * unScale;
    if (grassID == 1) vDims.y *= 1.5;
    float rotation = dimsTypeRotation.w * 6.28318530718; // 2 PI
    
    vec2 xDirection = vec2(cos(rotation), sin(rotation));
	
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
    vertexPosition.xyz += unPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
    vec3 xyzOffset = vec3(xzOffsetUncompressed.x, 0.0, xzOffsetUncompressed.y);
    // Rotate to surface normal
    xyzOffset.xy += xDirection * xyzOffset.x;
    xyzOffset = rotateOffsetToNormal(xyzOffset, normal);
 
	vertexPosition.xyz += xyzOffset.xyz;
	
	vec4 cameraRelativePos = vertexPosition - vec4(CameraPos, 0.0);
    
    // Wind
    vec2 randSeed = vec2(vPosition.xy);
    fWorldRoot = vPosition.xyz + unPosition;
    fRelXY = vPosition.xy;
    fHeight = xyzOffset.z;
	
	fDistance = length(cameraRelativePos.xy);
	
    fWorldPos = cameraRelativePos.xyz;
	
    // Blade type
    fGrassMaterial = grassID;
    int cellCounti = unGrassData[grassID].materialCellCount;
    float uWidth = 1.0 / float(cellCounti);
    float bladeType = floor(mod(rand(randSeed + vec2(3425.0, 2331.0)) * 255.0, cellCounti));
    
	// Grass blade uvs
    float randomFlip = rand(randSeed);
    // TODO: Only handles one row
	fUV = UVS[gl_VertexID % 4 + 4 * int(step(0.5, randomFlip))] * vec2(uWidth, 1.0);
    fUV.x += bladeType * uWidth;
    
    // Lean at the top
    fLean = xDirection * unLeanVariance * fUV.y * rand(randSeed) * unGrassData[grassID].leanVariance;
}