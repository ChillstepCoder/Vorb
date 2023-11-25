
// Draw one triangle to cover the entire screen. This is more efficient than
// a quad due to fewer fragment shader invocations
const vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));

uniform mat4 unInverseVP;
uniform vec2 unPosition = vec2(0.0);
uniform vec2 unSpawnPoint = vec2(0.5);

// Output
out vec2 fUV;
void main() {
    // [-1,1]
    mat2 smallIVP = mat2(unInverseVP);
    const vec2 xyPos = vertices[gl_VertexID];
    gl_Position = vec4(xyPos,0,1);
    fUV = (0.5 * (smallIVP * xyPos) + vec2(0.5)) + unPosition;
}