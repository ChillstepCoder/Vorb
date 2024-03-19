#include "stdafx.h"
#include "OwnershipGrid.h"

#include "world/markup/WorldMarkupGrid.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/component/SettlementComponents.h"

OwnershipGrid::OwnershipGrid(ui32 worldWidthTiles, WorldMarkupGrid& markupGrid) 
    : mMarkupGrid(markupGrid) {
    mWidthDTiles = worldWidthTiles / DTILE_WIDTH;
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    mTotalDTiles = SQ(mWidthDTiles);
    mChunkOwners = std::make_unique<ChunkOwnershipData[]>(SQ(mWidthChunks));
    mClaimedChunks.resizeAndZero(SQ(mWidthChunks));
}

OwnershipGrid::~OwnershipGrid() = default;

void OwnershipGrid::init(HostSimContext& simContext) {
    mSimContext = &simContext;
    mSimECS = &simContext.getECS();
}

entt::entity OwnershipGrid::getChunkOwner(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId].owner;
}

const ChunkOwnershipData& OwnershipGrid::getChunkSettlementOwnerData(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId];
}

DTileOwnershipData* OwnershipGrid::tryGetDTileOwnerDataForEditSimThread(DTileCoord dtilePosWorld) {
    ASSERT_SIM_THREAD();
    if (dtilePosWorld.x < 0 || dtilePosWorld.y < 0 || dtilePosWorld.x >= mWidthDTiles || dtilePosWorld.y >= mWidthDTiles) [[unlikely]] {
        return nullptr;
    }
    const ChunkID chunkId = (dtilePosWorld.y / CHUNK_WIDTH_DTILES) * mWidthChunks + (dtilePosWorld.x / CHUNK_WIDTH_DTILES);

    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (!data.dtileData) {
        return nullptr;
    }
    return &data.dtileData[(dtilePosWorld.y % CHUNK_WIDTH_DTILES) * CHUNK_WIDTH_DTILES + dtilePosWorld.x % CHUNK_WIDTH_DTILES];
}

const DTileOwnershipData* OwnershipGrid::tryGetDTileOwnerData(DTileCoord dtilePosWorld) const {
    ASSERT_SIM_THREAD();
    if (dtilePosWorld.x < 0 || dtilePosWorld.y < 0 || dtilePosWorld.x >= mWidthDTiles || dtilePosWorld.y >= mWidthDTiles) [[unlikely]] {
        return nullptr;
    }
    const ChunkID chunkId = (dtilePosWorld.y / CHUNK_WIDTH_DTILES) * mWidthChunks + (dtilePosWorld.x / CHUNK_WIDTH_DTILES);

    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (!data.dtileData) {
        return nullptr;
    }
    return &data.dtileData[(dtilePosWorld.y % CHUNK_WIDTH_DTILES) * CHUNK_WIDTH_DTILES + dtilePosWorld.x % CHUNK_WIDTH_DTILES];
}

const DTileOwnershipData* OwnershipGrid::tryGetDTileOwnerData(ChunkID chunkId, DTileIndex dtileIndex) const {
    ASSERT_SIM_THREAD();
    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (!data.dtileData) {
        return nullptr;
    }
    return &data.dtileData[dtileIndex];
}

bool OwnershipGrid::isDTileOwned(DTileCoord dtilePosWorld) const {
    ASSERT_SIM_THREAD();
    const DTileOwnershipData* data = tryGetDTileOwnerData(dtilePosWorld);
    if (!data) {
        return false;
    }
    return data->owner != entt::null;
}

bool OwnershipGrid::isChunkOwnedByAnySettlement(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId].owner != entt::null;
}

bool OwnershipGrid::isChunkOwnedBySettlement(ChunkID chunkId, entt::entity settlementId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId].owner == settlementId;
}

entt::entity OwnershipGrid::getChunkSettlementOwner(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId].owner;
}

void OwnershipGrid::setChunkOwner(ChunkID chunkId, entt::entity owner) {
    ASSERT_SIM_THREAD();
    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (data.owner != owner) {
        entt::registry& registry = mSimECS->getRegistrySimThread();
        if (data.owner != entt::null) {
            // Remove from old owner
            ChunkOwnershipComponent& cmp = registry.get<ChunkOwnershipComponent>(data.owner);
            for (size_t i = 0; i < cmp.ownedChunks.size(); ++i) {
                if (cmp.ownedChunks[i] == chunkId) {
                    cmp.ownedChunks[i] = cmp.ownedChunks.back();
                    cmp.ownedChunks.pop_back();
                    break;
                }
            }
        }
        registry.get_or_emplace<ChunkOwnershipComponent>(owner).ownedChunks.emplace_back(chunkId);
        data.owner = owner;
        allocateTileDataIfNeeded(data);
        mClaimedChunks.setBit(chunkId);
    }
}

void OwnershipGrid::setChunkOwnerIfUnowned(ChunkID chunkId, entt::entity owner) {
    ASSERT_SIM_THREAD();
    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (data.owner == entt::null) {
        entt::registry& registry = mSimECS->getRegistrySimThread();
        registry.get_or_emplace<ChunkOwnershipComponent>(owner).ownedChunks.emplace_back(chunkId);
        data.owner = owner;
        allocateTileDataIfNeeded(data);
        mClaimedChunks.setBit(chunkId);
    }
}

void OwnershipGrid::setDTileOwner(DTileCoord dtilePosWorld, entt::entity owner, DTileOwnerObjectType type, ui16 userData) {
    ASSERT_SIM_THREAD();
    if (dtilePosWorld.x < 0 || dtilePosWorld.y < 0 || dtilePosWorld.x >= mWidthDTiles || dtilePosWorld.y >= mWidthDTiles) [[unlikely]] {
        LOG_CRITICAL("Tried to set dtile owner at world pos {} {} OUT OF BOUNDS", dtilePosWorld.x, dtilePosWorld.y);
        return;
    }
    const ChunkID chunkId = (dtilePosWorld.y / CHUNK_WIDTH_DTILES) * mWidthChunks + (dtilePosWorld.x / CHUNK_WIDTH_DTILES);

    // Claim the chunk
    setChunkOwnerIfUnowned(chunkId, owner);

    ChunkOwnershipData& data = mChunkOwners[chunkId];
    DTileOwnershipData& tileData = data.dtileData[(dtilePosWorld.y % CHUNK_WIDTH_DTILES) * CHUNK_WIDTH_DTILES + dtilePosWorld.x % CHUNK_WIDTH_DTILES];
    tileData.owner = owner;
    tileData.userData = userData;
    tileData.ownerObjectType = type;
    if (owner != entt::null && mSimECS->getRegistrySimThread().try_get<SettlementDetailsComponent>(owner)) {
        tileData.flags.setBit(DTileOwnershipFlags::OwnedBySettlement);
    }
    else {
        tileData.flags.clearBit(DTileOwnershipFlags::OwnedBySettlement);
    }
}

bool OwnershipGrid::isChunkIsClaimed(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mClaimedChunks.getBit(chunkId);
}

void OwnershipGrid::claimChunk(ChunkID chunkId) {
    ASSERT_SIM_THREAD();
    assert(!mClaimedChunks.getBit(chunkId));
    mClaimedChunks.setBit(chunkId);
}

void OwnershipGrid::unclaimChunk(ChunkID chunkId) {
    ASSERT_SIM_THREAD();
    assert(mClaimedChunks.getBit(chunkId));
    mClaimedChunks.clearBit(chunkId);
}

bool OwnershipGrid::allocateTileDataIfNeeded(ChunkOwnershipData& data) {
    if (!data.dtileData) {
        data.dtileData = std::make_unique<DTileOwnershipData[]>(CHUNK_SIZE);
        return true;
    }
    return false;
}
