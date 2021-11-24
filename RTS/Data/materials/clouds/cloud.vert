#include "../GlobalUbo.glsl"
#include "../TboBillboardShared.glsl"

out vec2 fUV;
out vec2 fPosition;
flat out float fAtlasPage;
out vec4 fTint;
out float fRoughness;

void main() {

	vec4 vPosition = vec4(getPositionFromTbo(), 1.0);
	vec3 typeSize = getTypeSizeFromTbo();
	vec2 vDims = typeSize.yz;
	int type = int(typeSize.x);
	
	// Get uniform info
	vec3 atlasPageRoughnessWind = UnAtlasPageRoughnessWind[type].rgb;
	fRoughness = atlasPageRoughnessWind.g;
	
	// Compute uvs
    fUV = getUvsFromType(type);
    fAtlasPage = atlasPageRoughnessWind.r;
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
	vertexPosition.xyz += CameraUp * xzOffsetUncompressed.y;
	vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
	// Hacky way to make the x,z offsets all 1
	fPosition = clamp(xzOffsetUncompressed * 10.0, -1.0, 1.0);
	fPosition = (fPosition + 1.0) * 0.5; // 0 - 1 range

    fTint = vec4(1.0);
	
	vec4 worldPos = vertexPosition - vec4(CameraPos, 0.0);

	//fTint.r = 1.0 - angle;
	//fTint.g = 0.0;
	//fTint.b = 0.0;
	
	vec4 glPos = VP * worldPos;
    gl_Position = glPos;
	
}