#pragma once

constexpr ui64 PHYS_USER_DATA_BIT_COUNT = 3;

enum class PhysicsBodyUserDataType : ui8 {
    Terrain,
    Tile,
    Entity,
    ItemEntity,
    ContainerMesh,
    COUNT
};
static_assert(e_count(PhysicsBodyUserDataType) <= 8 /*2^3*/); // Fitting into 3 bits

constexpr ui64 PHYS_USER_DATA_TYPE_BIT_SHIFT = 64 - PHYS_USER_DATA_BIT_COUNT;
constexpr ui64 PHYS_USER_DATA_RIGHTPART_MASK = (ui64)0xFFFFFFFF;
// Part of user data shared with the type (After shifting)
constexpr ui64 PHYS_USER_DATA_LEFTPART_MASK_POSTSHIFT = PHYS_USER_DATA_RIGHTPART_MASK >> PHYS_USER_DATA_BIT_COUNT;
// Part of user data shared with the type
constexpr ui64 PHYS_USER_DATA_LEFTPART_MASK = PHYS_USER_DATA_LEFTPART_MASK_POSTSHIFT << 32;

struct PhysicsBodyUserData {
    PhysicsBodyUserData() = default;
    PhysicsBodyUserData(entt::entity owner);
    PhysicsBodyUserData(entt::entity owner, PhysicsBodyUserDataType type);
    PhysicsBodyUserData(TileContainerID tileContainer);
    PhysicsBodyUserData(TileContainerID tileContainer, TileIndex tileIndex);
    PhysicsBodyUserData(ui64 data) : data(data) {}
    PhysicsBodyUserData(PhysicsBodyUserDataType type);

    operator ui64() const { return std::bit_cast<ui64>(*this); }
    PhysicsBodyUserDataType getType() const;

    // Only works if getType == Tile
    std::pair<TileContainerID, TileIndex> getTileData() const;

    // Only works if getType == Tile || ContainerMesh
    TileContainerID getContainerId() const;

    // Only works if getType == Entity
    entt::entity getEntity() const;

    // We use bit packing to store lots of data in here
    ui64 data = 0;
};
static_assert(sizeof(PhysicsBodyUserData) == sizeof(ui64), "JoltUserData must be 64 bits");
