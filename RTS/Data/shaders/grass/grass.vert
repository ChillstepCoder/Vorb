
#include "../GlobalUbo.glsl"

uniform samplerBuffer UnTboPosition;
uniform samplerBuffer UnTboSizeType;
uniform vec3 unOffset;
uniform float UnYOffset = 1.0;
uniform vec2 unGrassScale;
uniform float unLeanVariance;

out vec3 fWorldPos;
flat out vec3 fWorldRoot;
out float fHeight;
out vec2 fUV;
flat out float fAtlasPage;
out float fDistance;
out vec2 fLean;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

// TODO: Match number of grass types
const float GRASS_TYPES_ROW_SIZE = 6;
const int GRASS_TYPES = 12;
const float GRASS_UV_X = 0.0833333333333; // 1/12
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
    
	vec2 vDims = dimsTypeRotation.xy * unGrassScale;
    float bladeType = round(dimsTypeRotation.z * 255.0);
    float rotation = dimsTypeRotation.w * 6.28318530718; // 2 PI
    
    vec2 xDirection = vec2(cos(rotation), sin(rotation));
	
    fAtlasPage = 0.0;
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.z += xzOffsetUncompressed.y;
	vertexPosition.xy += xDirection * xzOffsetUncompressed.x;
	
	vec4 cameraRelativePos = vertexPosition + vec4(unOffset, 0.0);
    
    // Wind
	vec3 trueWorldPos = vPosition.xyz + unOffset + CameraPos;
    fWorldRoot = trueWorldPos;
    fHeight = xzOffsetUncompressed.y;
	
	vec4 glPos = VP * cameraRelativePos;
	vec4 screenCamera = VP * vec4(CameraFront, 0.0);
	
	fDistance = length(cameraRelativePos.xy);
	
    fWorldPos = cameraRelativePos.xyz;
	
    
	// Grass blade uvs
    float randomFlip = rand(trueWorldPos.yx);
	fUV = UVS[gl_VertexID % 4 + 4 * int(step(0.5, randomFlip))];
    fUV.x += bladeType * GRASS_UV_X;
    
    // Lean at the top
    fLean = xDirection * unLeanVariance * fUV.y * rand(trueWorldPos.xy);
}