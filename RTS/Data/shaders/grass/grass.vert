
#include "../GlobalUbo.glsl"

uniform samplerBuffer UnTboPosition;
uniform samplerBuffer UnTboSizeType;
uniform vec3 unPosition;
uniform float UnYOffset = 1.0;
uniform vec2 unScale;
uniform float unLeanVariance;

const int NUM_GRASS_MATERIALS = 32;
uniform int unGrassMaterialCellCounts[NUM_GRASS_MATERIALS];
uniform float unGrassLeanVariance[NUM_GRASS_MATERIALS];
uniform vec2 unGrassScale[NUM_GRASS_MATERIALS];

out vec3 fWorldPos;
flat out vec3 fWorldRoot;
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
    int grassID = int(round(dimsTypeRotation.z * 255.0));
    
	vec2 vDims = dimsTypeRotation.xy * unGrassScale[grassID] * unScale;
    if (grassID == 1) vDims.y *= 1.5;
    float rotation = dimsTypeRotation.w * 6.28318530718; // 2 PI
    
    vec2 xDirection = vec2(cos(rotation), sin(rotation));
	
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
    vertexPosition.xyz += unPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xy += xDirection * xzOffsetUncompressed.x;
	
	vec4 cameraRelativePos = vertexPosition - vec4(CameraPos, 0.0);
    
    // Wind
    vec2 randSeed = vec2(vPosition.xy);
    fWorldRoot = vPosition.xyz + unPosition;
    fHeight = xzOffsetUncompressed.y;
	
	vec4 glPos = VP * cameraRelativePos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	fDistance = length(cameraRelativePos.xy);
	
    fWorldPos = cameraRelativePos.xyz;
	
    // Blade type
    fGrassMaterial = grassID;
    int cellCounti = unGrassMaterialCellCounts[grassID];
    float uWidth = 1.0 / float(cellCounti);
    float bladeType = round(mod(rand(randSeed + vec2(3425.0, 2331.0)) * 255.0, cellCounti));
    
    
	// Grass blade uvs
    float randomFlip = rand(randSeed);
    // TODO: Only handles one row
	fUV = UVS[gl_VertexID % 4 + 4 * int(step(0.5, randomFlip))] * vec2(uWidth, 1.0);
    fUV.x += bladeType * uWidth;
    
    // Lean at the top
    fLean = xDirection * unLeanVariance * fUV.y * rand(randSeed) * unGrassLeanVariance[grassID];
}