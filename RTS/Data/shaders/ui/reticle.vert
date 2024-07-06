
// Draw quad
const vec2 vertices[6]=vec2[6](
    vec2(0,0), vec2(1,0), vec2(1, 1),
    vec2(1,1), vec2(0,1), vec2(0, 0)
);

uniform float unSize;
uniform vec2 unScreenDims;

// Output
out vec2 fUV;
void main() {
    fUV = vec2(vertices[gl_VertexID]);
    
    vec2 screenPos = (fUV - vec2(0.5)) * vec2(unSize / unScreenDims.x, unSize / unScreenDims.y);
    
    gl_Position = vec4(screenPos,0,1);
}