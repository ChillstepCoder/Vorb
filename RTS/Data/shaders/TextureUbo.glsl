#extension GL_ARB_bindless_texture : enable
// This must not be modified, it is bound to code layout

layout(bindless_sampler) uniform;
layout (std140, binding = 1) uniform TextureUbo {
    vec3 unPosition;
    uvec4 Textures[256];
};
