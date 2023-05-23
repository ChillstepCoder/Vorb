
#include "GlobalUbo.glsl"
#include "GrassUbo.glsl"

uniform samplerBuffer UnTboPosition;
uniform samplerBuffer UnTboSizeType;
uniform samplerBuffer UnTboNormal;
uniform vec3 unPosition;
uniform float UnYOffset = 1.0;
uniform vec2 unScale;
uniform float unLeanVariance;

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

vec3 reconstructNormal(vec2 normalXY) {
    float den = max(1.0 - (normalXY.x * normalXY.x) - (normalXY.y * normalXY.y), 0.001);
    return vec3(normalXY, sqrt(den));
}

vec3 rotateOffsetToNormal(vec3 offset, vec3 normal) {
    vec3 up = vec3(0.0, 0.0, 1.0);
    // Compute the bending direction: perpendicular to the initial direction and the terrain normal
    vec3 bend_dir = cross(up, normal);

    // If the normal is already equal to the up vector, there is no bending to do
    if(length(bend_dir) < 0.0001) {
        return offset;
    }

    // Normalize the bending direction
    bend_dir = normalize(bend_dir);

    // Compute the final direction of the blade: perpendicular to the bending direction and the terrain normal
    vec3 blade_dir = cross(bend_dir, normal);

    // Compute the position of the blade
    vec3 position = offset.x * bend_dir + offset.y * blade_dir + offset.z * normal;

    return position;
}

void main() {
    int bladeIndex = (gl_VertexID / 4);
	vec4 vPosition = vec4(texelFetch(UnTboPosition, bladeIndex).rgb, 1.0);
	vec4 dimsTypeRotation = texelFetch(UnTboSizeType, bladeIndex);
    vec2 normal2 = (texelFetch(UnTboNormal, bladeIndex).rg * 2.0) - 1.0;
    //if (normal2.y <= -1.0) normal2.y = 0;
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