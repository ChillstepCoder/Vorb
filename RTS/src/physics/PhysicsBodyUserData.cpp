#include "stdafx.h"
#include "PhysicsBodyUserData.h"

PhysicsBodyUserData::PhysicsBodyUserData(entt::entity owner)
    : data(ui64(PhysicsBodyUserDataType::Entity) << PHYS_USER_DATA_TYPE_BIT_SHIFT | static_cast<ui64>(owner)) {
}

PhysicsBodyUserData::PhysicsBodyUserData(TileContainerID tileContainer)
    : data(ui64(PhysicsBodyUserDataType::ContainerMesh) << PHYS_USER_DATA_TYPE_BIT_SHIFT | static_cast<ui64>(tileContainer)) {
}

PhysicsBodyUserData::PhysicsBodyUserData(TileContainerID tileContainer, TileIndex tileIndex)
    : data(ui64(PhysicsBodyUserDataType::Tile) << PHYS_USER_DATA_TYPE_BIT_SHIFT | (static_cast<ui64>(tileIndex) << 32) | static_cast<ui64>(tileContainer)) {
    assert(tileIndex <= PHYS_USER_DATA_LEFTPART_MASK_POSTSHIFT); // 30 bits
}

PhysicsBodyUserData::PhysicsBodyUserData(PhysicsBodyUserDataType type)
    : data(ui64(type) << PHYS_USER_DATA_TYPE_BIT_SHIFT) {
}

PhysicsBodyUserDataType PhysicsBodyUserData::getType() const {
    return static_cast<PhysicsBodyUserDataType>(data >> PHYS_USER_DATA_TYPE_BIT_SHIFT);
}

std::pair<TileContainerID, TileIndex> PhysicsBodyUserData::getTileData() const {
    assert(getType() == PhysicsBodyUserDataType::Tile);
    std::pair<TileContainerID, TileIndex> result;
    result.first = static_cast<TileContainerID>(data & PHYS_USER_DATA_RIGHTPART_MASK);
    result.second = static_cast<TileIndex>((data >> 32) & PHYS_USER_DATA_LEFTPART_MASK_POSTSHIFT);
    return result;
}

TileContainerID PhysicsBodyUserData::getContainerId() const {
    assert(getType() == PhysicsBodyUserDataType::Tile || getType() == PhysicsBodyUserDataType::ContainerMesh);
    return static_cast<TileContainerID>(data & PHYS_USER_DATA_RIGHTPART_MASK);
}

entt::entity PhysicsBodyUserData::getEntity() const {
    assert(getType() == PhysicsBodyUserDataType::Entity);
    return static_cast<entt::entity>(data & PHYS_USER_DATA_RIGHTPART_MASK);
}