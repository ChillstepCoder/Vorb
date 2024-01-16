#pragma once

typedef ui32 ModelID;
constexpr ModelID INVALID_MODEL_ID = UINT32_MAX;

typedef GLuint64 TextureHandle;
typedef ui16 MaterialID;
constexpr ui16 INVALID_MATERIAL_ID = UINT16_MAX;

typedef ui64 StructureID;
constexpr StructureID INVALID_STRUCTURE_ID = UINT64_MAX;

typedef ui32 ItemStockpileID;
constexpr ItemStockpileID INVALID_ITEM_STOCKPILE_ID = UINT32_MAX;

typedef ui32 ContractID;
constexpr ContractID INVALID_CONTRACT_ID = UINT32_MAX;

typedef ui32 ParticleID;
constexpr ParticleID INVALID_PARTICLE_ID = UINT32_MAX;

typedef ui32 AssetID;
constexpr AssetID INVALID_ASSET_ID = UINT32_MAX;

typedef ui8 TileGrassID;
constexpr TileGrassID INVALID_TILE_GRASS_ID = UINT8_MAX;
constexpr ui32 MAX_TILE_GRASS_IDS = 0xff;

typedef ui32 HeightmapPatchID;
typedef ui32 ChunkID;
typedef ui32 LiteChunkID;
constexpr ui32 INVALID_CHUNK_ID = UINT32_MAX;

typedef ui32 WorldID;
constexpr WorldID INVALID_WORLD_ID = UINT32_MAX;

typedef ui32 FactionID;
constexpr ui32 INVALID_FACTION_ID = UINT32_MAX;

typedef ui32 ServerPlayerID;

typedef i32 TimestampCentisec;

// UIDs
typedef ui16 BusinessID; // No more than 65535 businesses per city
typedef ui32 CityUID; // We dont make many cities so ui32 is fine. We can always change it later
typedef ui64 CharacterUID;
typedef ui64 BuildingUID;