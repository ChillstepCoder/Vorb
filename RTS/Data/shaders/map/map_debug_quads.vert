
struct QuadData {
    vec2 pos;
    vec2 dims;
    vec4 color;
};

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

const int VertexIndex[6] = {
    0,
    1,
    2,
    2,
    3,
    0
};

// Output
out vec4 fColor;

uniform mat4 unVP;
uniform vec2 unCameraPos;

layout(std430, binding = 4) restrict readonly buffer QuadSSBO {
    QuadData quadData[];
};

QuadData getQuadData() {
  return quadData[(gl_VertexID / 6)];
}

vec2 getVertexOffsets() {
    vec2 vertexOffsets = VertexData[VertexIndex[gl_VertexID % 6]];
	vertexOffsets *= 0.5;
	return vertexOffsets;
}

void main() {
    QuadData data = getQuadData();
    
    vec4 vPosition = vec4(data.pos, 0.0, 1.0);
	vec2 vDims = data.dims;
	vec2 vertexOffsets = getVertexOffsets();
    vec2 xyOffset = vertexOffsets * vDims;
    
    fColor = data.color;
    gl_Position = unVP * vec4((vPosition.xy + xyOffset) - unCameraPos * 2.0, 0,1);
}