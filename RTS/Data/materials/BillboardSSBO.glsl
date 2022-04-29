#extension GL_ARB_bindless_texture : enable
// This must not be modified, it is bound to code layout

uniform float UnYOffset = 1.0;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

struct BillboardData {
   vec3 position;
   int texture;
   vec2 dims;
   // TODO: What else?
};

// UBO
struct BillboardTypeData {
    vec4 uvs;
    uvec4 texture;
};

layout (std140, binding = 1) uniform BillboardTypes {
  BillboardTypeData typeData[256];
};

layout(std430, binding = 2) buffer BillboardSSBO {
    BillboardData billboardData[];
};
