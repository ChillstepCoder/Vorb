#include "stdafx.h"
#include "HostSimContext.h"

#include "world/simulation/host/SimThread.h"
#include "world/World.h"
#include "world/IChunkGrid.h"
#include "world/simulation/host/SimECS.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"
#include "world/simulation/host/StoryTeller.h"
#include "world/simulation/host/SimImmigrationManager.h"
#include "ecs/IEntityComponentSystem.h"

#include "gamethread/GameThreadTasks.h"

HostSimContext::HostSimContext(World& world) :
    WorldContextObject(world),
    mChunkData(world.getTotalChunks()),
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
    mAnalytics->setDesiredPopulation(1000); // 10000

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

void HostSimContext::debugRender(f32v3 cameraPos) const {
    mSimECS->debugRender(cameraPos);
}

void HostSimContext::initEvents() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);

    //   Sim -> Game entity handshake process
    /*   CHUNK ACTIVATION
         1. Main thread activates chunk
         2. Sim thread listens to event
         - Incref chunk so it cant change
         - Add queued sim thread message for ActivateChunk
         3. SIM THREAD PROCESS ActivateChunk
         - Send message with all entities to be created
         4. Game thread process ENTITY_BATCH
         - Create all entities
         - Decref chunks

         CHUNK DEACTIVATION
         1. Main thread deactivates chunk and destroys
         2. Sim thread listens to event
         - Add queued sim thread message for DeactivateChunk
         - Set atomic flag on chunk "DO_NOT_ACTIVATE"
         - Remove all main thread entities
         3. SIM THREAD PROCESS
         - Reactivate sim controllers
         - Unset atomic flag on chunk DO_NOT_ACTIVATE

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
    */

    chunkGrid.addReadyListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        chunk.incRef();
        mSimThread->addTask([this, &chunk]() {
            mSimulatingChunks.clearBit(chunk.getChunkID());
            ChunkEntityFullActivateDataList entities = mSimECS->simThreadOnActivateChunk(chunk.getChunkID());
            GameThreadTasks::getInstance().addGenericTask([this, &chunk, entities = std::move(entities)]() {
                mWorld.getECS().createFullEntitiesFromSimEntities(chunk, entities);
                chunk.decRef();
            });
        });
    });
    chunkGrid.addDeactivateListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        // Tells main thread not to deactivate until we are done
        chunk.setState(ChunkState::DESTROYING_ON_SIM);
        mSimThread->addTask([this, &chunk]() {
            mSimulatingChunks.setBit(chunk.getChunkID());

            // Allow main thread to reactivate this chunk
            chunk.setState(ChunkState::INVALID);
        });
    });
}
