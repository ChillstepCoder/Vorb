#include "../GlobalUbo.glsl"
#include "../TboBillboardShared.glsl"

uniform vec3 UnRootPos;

out vec2 fUV;
out vec2 fPosition;
flat out float fAtlasPage;
out vec4 fTint;
out float fRoughness;
out mat3 fTBN;

vec3 rotateXY(vec3 inVec, float angle) {
    vec3 rv;
    float cosa = cos(angle);
    float sina = sin(angle);
    rv.x = cosa * inVec.x - sina * inVec.y;
    rv.y = sina * inVec.x + cosa * inVec.y;
    rv.z = inVec.z;
    
    return rv;
}

// https://www.neilmendoza.com/glsl-rotation-about-an-arbitrary-axis/
mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main() {

	vec4 vPosition = vec4(getPositionFromTbo(), 1.0);
	vec3 typeSize = getTypeSizeFromTbo();
	vec2 vDims = typeSize.yz;
	int type = int(typeSize.x);
	
	// Get uniform info
	vec3 atlasPageRoughnessWind = UnAtlasPageRoughnessWind[type].rgb;
	
	// Compute uvs
    fUV = getUvsFromType(type);
    fAtlasPage = atlasPageRoughnessWind.r;
	
	
	// Compute position
	vec2 vertexOffsets = getVertexOffsets();
	vec4 vertexPosition = vPosition;
	vec2 xzOffsetUncompressed = vertexOffsets * vDims; // Matches C++ compression ratio
    
    // Movement offset
	vertexPosition.xyz += cos((vPosition.x + UnRootPos.x) * 0.05 - sin((vPosition.y + UnRootPos.y) * 0.05)) * 10.0;
    
    // Get Right and Up vectors in world space
    vec3 cameraNormal = vertexPosition.xyz + UnRootPos - CameraPos;
    cameraNormal = normalize(cameraNormal);
    vec3 worldRight = normalize(vec3(rotateXY(cameraNormal, -90.0 * (3.141592653 / 180.0)).xy, 0.0));
    vec3 worldUp = normalize((rotationMatrix(worldRight, -90.0) * vec4(cameraNormal, 1.0)).xyz);
    
	vertexPosition.xyz += worldUp * xzOffsetUncompressed.y;
	vertexPosition.xyz += worldRight * xzOffsetUncompressed.x;
    
    // OLD CAMERA SPACE
	//vertexPosition.xyz += CameraUp * xzOffsetUncompressed.y;
	//vertexPosition.xyz += CameraRight * xzOffsetUncompressed.x;
    
    vec3 normal = -cameraNormal; // Prenormalized on CPU
	vec3 binormal = -worldUp;
    vec3 tangent = worldRight;
	fTBN = mat3(tangent, binormal, normal);
    
	// Hacky way to make the x,z offsets all 1
	fPosition = clamp(xzOffsetUncompressed * 10.0, -1.0, 1.0);
	fPosition = (fPosition + 1.0) * 0.5; // 0 - 1 range

    fTint = vec4(1.0);
	
	vec4 worldPos = vertexPosition + vec4(UnRootPos - CameraPos, 0.0);

	//fTint.r = 1.0 - angle;
	//fTint.g = 0.0;
	//fTint.b = 0.0;
	
	vec4 glPos = VP * worldPos;
    gl_Position = glPos;
	
}