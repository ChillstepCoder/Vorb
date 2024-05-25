#include "stdafx.h"
#include "HostSimContext.h"

#include "world/simulation/host/SimThread.h"
#include "world/World.h"
#include "world/IChunkGrid.h"
#include "world/simulation/host/SimECS.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"
#include "world/simulation/host/StoryTeller.h"
#include "world/simulation/host/SimImmigrationManager.h"
#include "world/chunk/SimChunkTileGrid.h"
#include "ecs/IEntityComponentSystem.h"

#include "gamethread/GameThreadTasks.h"

HostSimContext::HostSimContext(World& world) :
    WorldContextObject(world),
    mTotalChunks(world.getTotalChunks())
{
    mAnalytics = std::make_unique<SimWorldAnalytics>();
    mStoryTeller = std::make_unique<StoryTeller>();
    mChunkStates.resizeAndZero(mTotalChunks);
    mSimulatingChunks.resize(mTotalChunks);
    mSimulatingChunks.fill(true);
    mSimECS = std::make_unique<SimECS>(*this);
    mImmigrationManager = std::make_unique<SimImmigrationManager>(*this);
}

HostSimContext::~HostSimContext() {
    mSimThread.reset(); // Join
}

void HostSimContext::beginHistorySimulation() {

    mImmigrationManager->init();
    mAnalytics->setDesiredPopulation(3000); // 10000

    assert(!mSimThread);
    assert(!mSimulatingHistory);
    mSimThread = std::make_unique<SimThread>(*this, mWorld);
    mSimThread->setState(SimThreadState::HistorySim);
    mSimThread->setTargetTickRateMs(1.0);
    mSimThread->setTimeScale(1000000.0f); // 100000x speed sim (currently 10x)
    mSimThread->start();
    mSimulatingHistory = true;
}

void HostSimContext::endHistorySimulation() {
    // TODO: We should probably join here
    mSimThread->setState(SimThreadState::Idle);
}

void HostSimContext::onWorldBeginGame() {
    if (!mSimThread) {
        mSimThread = std::make_unique<SimThread>(*this, mWorld);
    }
    mSimThread->setTargetTickRateMs(200.0);
    mSimThread->setTimeScale(1.0f);
    mSimThread->setState(SimThreadState::GameSim);

    initEvents();
}

void HostSimContext::registerPlayer(ServerPlayerID playerId, f32v3 startPos) {
    SimPlayer newPlayer;
    newPlayer.mPlayerId = playerId;
    newPlayer.mLastKnownPosition = startPos;
    mPlayers.push_back(newPlayer);
}

void HostSimContext::setPlayerPosition(ServerPlayerID playerId, f32v3 pos) {
    for (auto&& p : mPlayers) {
        if (p.mPlayerId == playerId) {
            p.mLastKnownPosition = pos;
            return;
        }
    }
}

void HostSimContext::removePlayer(ServerPlayerID playerId) {
    for (auto&& it = mPlayers.begin(); it != mPlayers.end(); ++it) {
        if (it->mPlayerId == playerId) {
            mPlayers.erase(it);
            return;
        }
    }
}

RandomGenerator& HostSimContext::getSimRandomGenerator() const {
    return mSimThread->getRandomGenerator();
}

ui32 HostSimContext::getWidthChunks() const {
    return mWorld.getChunkGrid().getWidthChunks();
}

bool HostSimContext::isChunkSimulating(ChunkID chunkId) const {
    return mSimulatingChunks.getBit(chunkId);
}

void HostSimContext::debugRender(f32v3 cameraPos) const {
    mSimECS->debugRender(cameraPos);
}

void HostSimContext::initEvents() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);

    // TODO: UPDATE THIS COMMENT
    //   Sim -> Game entity handshake process
    /*   CHUNK ACTIVATION
         1. Main thread activates chunk
         2. Sim context listens to event
         - Incref chunk so it cant change
         - Add queued sim thread message for ActivateChunk
         3. SIM THREAD PROCESS ActivateChunk
         - Create all SimFullEntityBinding 
         - Send message with all entities to be created
         4. Game thread process ENTITY_BATCH
         - Create all entities
         - Finalize SimFullEntityBinding
         - Decref chunks

         CHUNK DEACTIVATION
         1. Main thread deactivates chunk and destroys
         2. Sim context listens to event
         - Add queued sim thread message for DeactivateChunk
         - Set atomic state on chunk "DESTROYING_ON_SIM"
         - Remove all main thread entities
         3. SIM THREAD PROCESS
         - Reactivate sim controllers
         - Unset atomic state on chunk DESTROYING_ON_SIM
         - Delete binding, handle any state that was unconsumed by the main thread

         ENTITY ENTER FULL CHUNK
         1. On sim entity enter full chunk
         2. Send ActivateEntity to game thread
         3. Game thread checks if chunk still valid
         - Is valid? Create entity
         - Not valid? Send create entity message back to sim thread

         ENTITY LEAVE FULL CHUNK
         1. On game entity leave full chunk
         2. Send ActivateEntity to sim thread
         3. Sim thread checks if chunk still simulated
         - Is valid? create entity
         - Not valid? send create entity message back to game thread

          BINDING LOGIC
          SimFullEntityBinding is what allows the sim thread to push state to the game thread
          This is a one way relationship, no need for mutex if we use queue
    */

    chunkGrid.addBeginActivateListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        // We have guarantee that the chunk cannot be destroyed while we have this state
        assert(chunk.getState() == ChunkState::WAITING_SIM_RELEASE);

        mSimThread->addTask([this, &chunk]() {
            mSimulatingChunks.clearBit(chunk.getChunkID());
            ChunkEntityFullActivateDataList entities = mSimECS->simThreadOnActivateChunk(chunk.getChunkID());

            mWorld.getSimTileGrid().getChunk(chunk.getChunkID()).bindEditEventToChunkTileContainer(chunk);
            GameThreadTasks::getInstance().addGenericTask([this, &chunk, entities = std::move(entities)]() mutable {
                mWorld.getECS().addPendingEntitiesToChunk(chunk, std::move(entities));
                chunk.setState(ChunkState::READY_TO_LOAD);
            });
        });
    });

    chunkGrid.addDeactivatedListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        // Tells main thread not to activate until we are done
        chunk.setState(ChunkState::DESTROYING_ON_SIM);
        ChunkEntityFullDeactivateDataList deactivateList = mWorld.getECS().deactivateEntitiesForChunk(chunk);

        mWorld.getSimTileGrid().getChunk(chunk.getChunkID()).unBindEditEventToChunkTileContainer();
        mSimThread->addTask([this, &chunk, deactivateList = std::move(deactivateList)]() {
            mSimulatingChunks.setBit(chunk.getChunkID());
            mSimECS->simThreadOnFullDeactivateEntities(chunk.getChunkID(), deactivateList);

            // Allow main thread to reactivate this chunk
            chunk.setState(ChunkState::DEACTIVATED);
        });
    });

    IEntityComponentSystem& fullEcs = mWorld.getECS();
    fullEcs.registerIEntityComponentSystemListeners(mFullECSListeners);

    fullEcs.addEntityDeactivatedListener(mFullECSListeners, [this](FullECSEvent evnt) {
        ASSERT_GAME_THREAD();

        mSimThread->addTask([this, chunkId = evnt.chunkId, dData = evnt.deactivateData]() {
            if (mSimulatingChunks.getBit(chunkId)) {
                mSimECS->simThreadOnFullDeactivateEntity(chunkId, dData);
            }
            else {
                // Need to send it back, we don't own control
                mSimECS->onEntityDeactivationFailed(chunkId, dData);
            }
        });
    });
}
