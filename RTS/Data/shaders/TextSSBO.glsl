#extension GL_ARB_bindless_texture : enable
// This must not be modified, it is bound to code layout

uniform float UnYOffset = 1.0;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

struct GlyphData {
   vec4 uvs;
   vec3 origin;
   int padding;
   vec2 dims;
   vec2 xyOffset;
};

layout(std430, binding = 3) readonly buffer BillboardSSBO {
    GlyphData glyphData[];
};
