#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

// This must not be modified, it is bound to code layout

layout(bindless_sampler) uniform;
layout (std140, binding = 1) uniform TextureUbo {
    vec3 unPosition;
    uvec4 Textures[256];
};
