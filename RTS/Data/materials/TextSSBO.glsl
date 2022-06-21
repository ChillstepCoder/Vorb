#extension GL_ARB_bindless_texture : enable
// This must not be modified, it is bound to code layout

uniform float UnYOffset = 1.0;
uniform uvec4 unFontTexture;

const vec2 VertexData[4] = {
 {-1.0, -1.0 },
 {1.0,  -1.0 },
 {1.0,   1.0 },
 {-1.0,  1.0 }
};

struct GlyphData {
   vec4 uvs;
   vec3 origin;
   int texture;
   vec2 dims;
   vec2 xyOffset;
};

layout (std140, binding = 1) uniform BillboardTypes {
  uvec4 fontTextures[256];
};

layout(std430, binding = 2) buffer BillboardSSBO {
    GlyphData glyphData[];
};
