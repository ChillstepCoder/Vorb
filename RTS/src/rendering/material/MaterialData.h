#pragma once

constexpr const TextureHandle INVALID_TEXTURE_HANDLE = 0;

#include "rendering/model/MaterialRenderPassType.h"

enum MaterialFlags {
    MaterialFlags_CastShadow = BIT(0),
    MaterialFlags_ReceiveShadow = BIT(1),
    MaterialFlags_Transparent = BIT(2),
};

struct PACKED_STRUCT MaterialGpuData final {
    f32v4 emissiveColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32v4 albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // UV anisotropic roughness (isotropic lighting models use only the first value). ZW values are ignored
    f32v4 roughness = { 1.0f, 1.0f, 0.0f, 0.0f };

    f32 transparencyFactor = 1.0f; // UNUSED
    f32 alphaTest = 0.01f; // UNUSED
    f32 metallicFactor = 0.0f;

    ui32 flags = MaterialFlags_CastShadow | MaterialFlags_ReceiveShadow;
    // Albedo (RGB) Alpha (A)
    TextureHandle albedoMap = INVALID_TEXTURE_HANDLE;
    // Normal (RGB)
    TextureHandle normalMap = INVALID_TEXTURE_HANDLE;
    // Height (R)
    TextureHandle displacementMap = INVALID_TEXTURE_HANDLE;
    /// AO (R), Roughness (G), Metallic (B) https://github.com/KhronosGroup/glTF/issues/857
    TextureHandle aoMetallicRoughnessMap = INVALID_TEXTURE_HANDLE;
};
static_assert(sizeof(MaterialGpuData) % 16 == 0, "MaterialData should be padded to 16 bytes");

struct MaterialData {
    MaterialID id = INVALID_MATERIAL_ID;
    MaterialRenderPassType renderPass = MaterialRenderPassType::Default;
};
static_assert(sizeof(MaterialData) == 4);

struct MaterialHandle {
    bool isValid() const { return data != nullptr; }

    MaterialID materialId = INVALID_MATERIAL_ID;
    MaterialGpuData* data = nullptr;
    nString name;
};
