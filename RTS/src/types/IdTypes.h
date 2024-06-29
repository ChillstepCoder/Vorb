#pragma once

typedef ui32 ModelID;
constexpr ModelID INVALID_MODEL_ID = std::numeric_limits<ModelID>::max();

typedef ui32 RegionID;
constexpr RegionID INVALID_REGION_ID = std::numeric_limits<RegionID>::max();

typedef GLuint64 TextureHandle;
typedef ui16 MaterialID;
constexpr ui16 INVALID_MATERIAL_ID = std::numeric_limits<MaterialID>::max();

typedef ui32 BuildingID;
constexpr BuildingID INVALID_BUILDING_ID = std::numeric_limits<BuildingID>::max();

typedef ui32 ItemStockpileID;
constexpr ItemStockpileID INVALID_ITEM_STOCKPILE_ID = std::numeric_limits<ItemStockpileID>::max();

typedef ui32 ContractID;
constexpr ContractID INVALID_CONTRACT_ID = std::numeric_limits<ContractID>::max();

typedef ui32 ParticleID;
constexpr ParticleID INVALID_PARTICLE_ID = std::numeric_limits<ParticleID>::max();

typedef ui32 AssetID;
constexpr AssetID INVALID_ASSET_ID = std::numeric_limits<AssetID>::max();

typedef ui8 TileGrassID;
constexpr TileGrassID INVALID_TILE_GRASS_ID = std::numeric_limits<TileGrassID>::max();
constexpr ui32 MAX_TILE_GRASS_IDS = 0xff;

typedef ui32 HeightmapPatchID;
typedef ui32 ChunkID;
constexpr ui32 INVALID_CHUNK_ID = std::numeric_limits<ChunkID>::max();

typedef ui32 WorldID;
constexpr WorldID INVALID_WORLD_ID = std::numeric_limits<WorldID>::max();

typedef ui32 FactionID;
constexpr ui32 INVALID_FACTION_ID = std::numeric_limits<FactionID>::max();

typedef ui32 ServerPlayerID;

typedef ui64 TimestampMs;

typedef ui32 BodyID;
constexpr ui32 INVALID_BODY_ID = std::numeric_limits<BodyID>::max();

typedef ui32 RoadSegmentID;
constexpr RoadSegmentID INVALID_ROAD_SEGMENT_ID = std::numeric_limits<RoadSegmentID>::max();

typedef ui32 SettlementPlotID;
constexpr SettlementPlotID INVALID_SETTLEMENT_PLOT_ID = std::numeric_limits<SettlementPlotID>::max();

typedef ui16 BusinessID; // No more than 65535 businesses per city
constexpr ui16 INVALID_BUSINESS_ID = std::numeric_limits<BusinessID>::max(); static_assert(sizeof(BusinessID) == sizeof(ui16));

typedef ui32 FamilyID;
constexpr ui32 INVALID_FAMILY_ID = std::numeric_limits<FamilyID>::max();

typedef ui16 ItemID;
constexpr ItemID INVALID_ITEM_ID = std::numeric_limits<ItemID>::max();

typedef ui64 TileItemUID;
constexpr TileItemUID INVALID_TILE_ITEM_UID = std::numeric_limits<TileItemUID>::max();

typedef ui32 StaticModelInstanceID;
constexpr StaticModelInstanceID INVALID_STATIC_MODEL_INSTANCE_ID = std::numeric_limits<StaticModelInstanceID>::max();

using PhysBodyID = ui32;
constexpr PhysBodyID INVALID_PHYS_BODY_ID = std::numeric_limits<PhysBodyID>::max();

using NavPathID = ui16;
constexpr NavPathID INVALID_NAV_PATH_ID = 0;

// UIDs
typedef ui32 CityUID; // We dont make many cities so ui32 is fine. We can always change it later
typedef ui32 SettlementUID;
typedef ui64 CharacterUID;
typedef ui64 BuildingUID;

constexpr ui32 INVALID_CITY_UID = std::numeric_limits<CityUID>::max(); static_assert(sizeof(CityUID) == sizeof(ui32));
constexpr ui32 INVALID_SETTLEMENT_UID = std::numeric_limits<SettlementUID>::max(); static_assert(sizeof(SettlementUID) == sizeof(ui32));
constexpr ui64 INVALID_BUILDING_UID = std::numeric_limits<BuildingUID>::max(); static_assert(sizeof(BuildingUID) == sizeof(ui64));
constexpr ui64 INVALID_CHARACTER_UID = std::numeric_limits<CharacterUID>::max(); static_assert(sizeof(CharacterUID) == sizeof(ui64));

