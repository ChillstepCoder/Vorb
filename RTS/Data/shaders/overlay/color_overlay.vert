
// Draw one triangle to cover the entire screen. This is more efficient than
// a quad due to fewer fragment shader invocations
const vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));

// Output
out vec2 fUV;
void main() {
    gl_Position = vec4(vertices[gl_VertexID],0,1);
    fUV = 0.5 * gl_Position.xy + vec2(0.5);
}