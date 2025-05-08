#pragma once
#include "ecs/component/Components.h"

#include "ecs/component/FishingComponent.h"
#include "ecs/ChunkFullTransitionData.h"
#include "ecs/FullECSEvents.h"
#include "ecs/AttachedEntityUpdateHandle.h"

#include "world/ChunkGridEvent.h"
#include "world/WorldEvents.h"

#include "physics/PhysicsWorldEvents.h"

class PlayerInteractSystem;

class AttachedEntityUpdater;

#include <mutex>

class World;

class IFullECS {
public:
    IFullECS(World& world);
    virtual ~IFullECS();

    void onWorldBeginGameThread();

    virtual void tick(f32 elapsedSec);
    virtual void tickPhysics(f32 elapsedSec);

    // Create an entity, on server it will optionally replicate, on client it cannot replicate
    virtual entt::entity createEntity(const f32v3& position, StrToken typeToken, bool shouldReplicate) = 0;
    // Client or server
    virtual void destroyEntity(entt::entity entity) = 0;

    void addPendingEntitiesToChunk(LocalChunk& chunk, ChunkFullTransitionData&& data);
    void createFullEntitiesFromSimEntities(LocalChunk& chunk, ChunkFullTransitionData& data);
    ChunkSimTransitionData deactivateEntitiesForChunk(LocalChunk& chunk);

    // Returns true if the entity was destroyed
    bool onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunkID, ChunkID newChunkID);

    entt::entity getLocalPlayer() const { ASSERT_GAME_THREAD(); return mLocalPlayerEntity; }
    entt::entity createLocalPlayer(f32v3 position, ServerPlayerID playerId);
    f32v3 getLocalPlayerPosition();

    // DEBUG:
    // Update will be removed when caller drops the handle
    void addThreadSafeEntityUpdateForNearestCharacter(AttachedEntityUpdateHandlePtr updateHandle, f32v3 pos);


    // TODO: Move to some ItemSystem or something
    // Returns remaining quantity
    i32 pickupTileItem(entt::entity picker, TileItemUID itemUID, i32 quantity);
    i32 pickupDynamicItem(entt::entity picker, entt::entity itemEntity, i32 quantity);

    // TODO: UniquePtr for faster include
    CharacterControlSystem mCharacterControlSystem;
    std::unique_ptr<PlayerControlSystem> mPlayerControlSystem;
    std::unique_ptr<PlayerInteractSystem> mPlayerInteractSystem; // TODO: DELETE
    TimedTileInteractSystem mTimedTileInteractSystem;
    PhysicsSystem mPhysicsSystem;
    CameraAttachSystem mCameraAttachSystem;
    FishingComponentSystem mFishingSystem;
    SkillsComponentSystem mSkillsSystem;


	// Classes with World access
	friend class PhysicsComponent;

    World& mWorld;

    entt::registry mRegistry;

    std::unique_ptr<AttachedEntityUpdater> mDebugEntityUpdater;

    EVENT_LISTENER_FUNCS(IFullECS, EntityDeactivated, FullECSEventType::EntityDeactivated, FullECSEvent&);

protected:

    void initEvents();
    void connectItemToChunk(entt::entity entity);

    // TODO: Periodically shrink_to_fit
    std::vector<EntityVector> mEntitiesByChunk;
    // Map of tile item UID to entity
    UnorderedFlatMap<TileItemUID, entt::entity> mTileItemEntityMap;
    // Entities that are waiting for chunk to load so they can activate
    std::map<ChunkID, ChunkFullTransitionData> mPendingEntities;

    ChunkGridListeners mChunkEventListeners;
    WorldListeners mWorldEventListeners;
    PhysicsWorldListeners mPhysicsWorldListeners;

    entt::entity mLocalPlayerEntity = entt::null;


    EVENT_DISPATCHER_DEF(IFullECS);
};