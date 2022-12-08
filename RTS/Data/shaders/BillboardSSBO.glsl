
uniform float UnYOffset = 1.0;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

// This must not be modified, it is bound to code layout
struct BillboardData {
   vec3 position;
   float xFlip;
   vec2 dims;
   uint material;
};

layout(std430, binding = 3) restrict readonly buffer BillboardSSBO {
    BillboardData billboardData[];
};
