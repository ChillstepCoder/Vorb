#include "stdafx.h"
#include "NavigationComponent.h"

#include "ecs/IFullECS.h"

#include "debugging/DebugRenderer.h"

#include "options/DebugOptions.h"
#include "pathfinding/NavThread.h"
#include "pathfinding/NavPath.h"

#include "world/IChunkGrid.h"

#include "rendering/RenderThreadTasks.h"

#include <glm/gtx/rotate_vector.hpp>

#include "world/World.h"

constexpr int RAYCHECK_INTERVAL_FRAMES = 4;
constexpr float MIN_DISTANCE = 0.5f; // TODO: This used to be 0.9, extra large to account for steering to steer around obstacles
//constexpr int QUADRANTS = 5; //bad name

bool NavigationSystem::updateComponentSimpleLinear(entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, f32v3 pos) {
	const f32v2 offset2d = navCmp.mTargetPosition - pos;
    const float distance2 = glm::length2(offset2d);
    if (distance2 <= SQ(MIN_DISTANCE)) {
		motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
        return true;
    }

	// TODO: Allow variable pathing urgency
    motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::SPRINT;
	motionCmp.mMoveDirection = (offset2d / std::sqrt(distance2));
	return false;
}

NavigationSystem::PathStatus NavigationSystem::updateComponentFinePath(World& world, entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, f32v3 pos) {

	if (!navCmp.mFinePath->finishedGenerating.load()) {
		return PathStatus::IN_PROGRESS;
	}
	else {
		navCmp.mTargetPosition = navCmp.mFinePath->getTargetPosition();
	}

    const ui32 numPoints = navCmp.mFinePath->getNumPoints();
	if (numPoints == 0) {
		return PathStatus::FAIL;
	}

    if (navCmp.mCurrentFinePoint >= numPoints) {
		return PathStatus::SUCCESS;
	}

	const f32v3* points = navCmp.mFinePath->getPoints();
	const f32v3 nextPoint = points[navCmp.mCurrentFinePoint];
	

	// Check for stuck on new tile/jump
	const TileHandle tileHandle = world.getTileHandleAtWorldPos(nextPoint);
	if (tileHandle.isValid()) {
        const f32 baseZ = tileHandle.getTile().getGroundZOffset();
        if (baseZ >= pos.z + 0.1f /*1.1*/) {
            // Climb
			motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::JUMPING;
        }
	}

	// TODO: This will struggle in buildings potentially as it is a 2d check and stairs are 3d
	const f32v2 offset2d = nextPoint - pos;
	const float distance2 = glm::length2(offset2d);
	if (distance2 <= SQ(MIN_DISTANCE)) {
        ++navCmp.mCurrentFinePoint;
        if (navCmp.mCurrentFinePoint >= numPoints) {
			// Target reached
			if (navCmp.mFinePath->getSimChunkEndPoint() != INVALID_CHUNK_ID) {
				assert(false); // HANDLE THIS
            }
            navCmp.mFinePath = nullptr;
			return PathStatus::SUCCESS;
		}
	}

	motionCmp.mMoveDirection = (offset2d / std::sqrt(distance2)) /* * (cmp.mColliding ? 0.2f : 1.0f)*/;
    // TODO: Allow variable pathing urgency
    motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::SPRINT;
	    
	return PathStatus::IN_PROGRESS;
}

void NavigationSystem::onPathingFinished(NavigationComponent& navCmp, CharacterControlComponent& motionCmp, bool success) {
	// Target reached
	motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
	motionCmp.mMoveDirection = f32v2(0.0f);
    if (success) {
		if (navCmp.mTargetPosition == f32v3(0.0f)) {
			if (navCmp.mFinePath) {
				navCmp.mTargetPosition = navCmp.mFinePath->getTargetPosition();
			}
			else if (navCmp.mCoarsePath) {
                navCmp.mTargetPosition = navCmp.mCoarsePath->getTargetPosition();
			}
		}
		assert(navCmp.mTargetPosition != f32v3(0.0f));
		navCmp.mStatus = NavigationStatus::SUCCESS;
	}
    else {
		navCmp.mStatus = NavigationStatus::FAIL;
    }
    navCmp.mFinePath = nullptr;
    navCmp.mCoarsePath = nullptr;
    navCmp.mNavigationType = NavigationType::INVALID;
}

void NavigationSystem::requestFinePathToPoint(World& world, NavigationComponent& navCmp, f32v3 start, f32v3 goal) {
    navCmp.mPendingFinePath = std::make_shared<NavPath>();
	if (sDebugOptions.mShowPaths) {
		// Make sure we dont free this path before it is rendered
        std::shared_ptr<NavPath> pathHandle = navCmp.mPendingFinePath;
        assert(Services::isUsingNav());
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, start, goal, false /*isCoarse*/, [pathHandle, &world]() {

			std::vector<f32v3>* pointsHandle = new std::vector<f32v3>(std::move(pathHandle->convertToWorldPoints(world.getHeightmapGrid())));

			RenderThreadTasks::getInstance().addGenericTask([](RenderContext&, void* vPathHandle) {
				std::vector<f32v3>* pathHandle = static_cast<std::vector<f32v3>*>(vPathHandle);
				DebugRenderer::drawPath(*pathHandle, color4(1.0f, 0.0f, 1.0f), 200);
				delete pathHandle;
			}, pointsHandle);
		});
	}
    else {
        assert(Services::isUsingNav());
		Services::NavThread::ref().addPathfindTask(navCmp.mPendingFinePath, start, goal, false /*isCoarse*/, nullptr);
	}
}

bool NavigationSystem::isPathStatusDone(PathStatus pathStatus)
{
    return pathStatus > PathStatus::IN_PROGRESS;
    static_assert(e_cast(PathStatus::COUNT) == 3);
}

void NavigationSystem::updateComponentCoarsePath(World& world, entt::entity entity, NavigationComponent& navCmp, CharacterControlComponent& motionCmp, f32v3 pos) {

    if (!navCmp.mCoarsePath->finishedGenerating.load()) {
        return;
	}

	const ui32 numPoints = navCmp.mCoarsePath->getNumPoints();
    if (numPoints == 0) {
        onPathingFinished(navCmp, motionCmp, false /*success*/);
        return;
	}

    // Lazy initialize the coarse point to a look-ahead position for better pathing
    if (navCmp.mCurrentCoarsePoint == 0) {
		if (numPoints > 2) {
			navCmp.mCurrentCoarsePoint = 2;
		}
        else if (numPoints > 0) {
            navCmp.mCurrentCoarsePoint = numPoints - 1;
		}
    }

    if (navCmp.mCurrentCoarsePoint >= numPoints) {
		onPathingFinished(navCmp, motionCmp, true /*success*/);
        return;
    }

	const f32v3* points = navCmp.mCoarsePath->getPoints();

	f32v3 nextCoarseTilePos = points[navCmp.mCurrentCoarsePoint] + f32v3(0.5f, 0.5f, 0.0f);

	const bool hasFinePath = navCmp.mPendingFinePath || navCmp.mFinePath;

    if (!hasFinePath) {
        // We need a path
        navCmp.mCurrentFinePoint = 0;
		requestFinePathToPoint(world, navCmp, pos, nextCoarseTilePos);

    }
    else {
		// Our fine path finished generating
		if (navCmp.mPendingFinePath && navCmp.mPendingFinePath->finishedGenerating) {
			navCmp.mCurrentFinePoint = 0;
			navCmp.mFinePath = std::move(navCmp.mPendingFinePath);
			navCmp.mPendingFinePath.reset();
		}

		if (navCmp.mFinePath) {
			bool requestNextPath = false;
			assert(navCmp.mFinePath->finishedGenerating.load());
			const PathStatus fineStatus = updateComponentFinePath(world, entity, navCmp, motionCmp, pos);
			if (fineStatus == PathStatus::SUCCESS) {
                navCmp.mFinePath = nullptr;
				requestNextPath = true;
			}
			else if (fineStatus == PathStatus::FAIL) {
                onPathingFinished(navCmp, motionCmp, false /*success*/);
			}
			else if ((navCmp.mCurrentCoarsePoint < numPoints - 1) && (navCmp.mCurrentFinePoint > navCmp.mFinePath->getNumPoints() / 2u)) {
				// Halfway through path we start generating next path
				// TODO: This causes a bug where it does this forever, check logic ^
				requestNextPath = true;
			}

			if (requestNextPath) {
                ++navCmp.mCurrentCoarsePoint;
                if (navCmp.mCurrentCoarsePoint >= numPoints) {
					const ChunkID simEndChunk = navCmp.mCoarsePath->getSimChunkEndPoint();
					if (simEndChunk != INVALID_CHUNK_ID) {
						// Check if our target sim chunk is still a sim chunk or if we should re-path if its valid
						if (world.getChunkGrid().getChunk(simEndChunk).isActivated()) {
							navCmp.requestCoarsePath(pos, navCmp.mCoarsePath->getTargetPosition());
						}
						else {
							navCmp.setSimpleLinearTargetPoint(navCmp.mCoarsePath->getTargetPosition());
						}
					}
					else {
						onPathingFinished(navCmp, motionCmp, true /*success*/);
					}
					return;
                }
                else {
					// Path forward
                    nextCoarseTilePos = points[navCmp.mCurrentCoarsePoint];
                    requestFinePathToPoint(world, navCmp, pos, nextCoarseTilePos);
                }
			}
		}
		// TODO: Check LOS issues
    }
}

void NavigationSystem::update(World& world, entt::registry& registry) {
	// Update components
    auto view = registry.view<NavigationComponent, PositionComponent, CharacterControlComponent>();


    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        auto& posCmp = view.get<PositionComponent>(entity);
        auto& controlCmp = view.get<CharacterControlComponent>(entity);
		const f32v3 position = posCmp.mPosition;

        switch (navCmp.mNavigationType) {
			case NavigationType::FINE_PATH: {
				const PathStatus fineStatus = updateComponentFinePath(world, entity, navCmp, controlCmp, position);
				if (isPathStatusDone(fineStatus)) {
					onPathingFinished(navCmp, controlCmp, fineStatus == PathStatus::SUCCESS ? true : false);
				}
				break;
			}
            case NavigationType::COARSE_PATH:
				// TODO: Are these checks pointless?
				updateComponentCoarsePath(world, entity, navCmp, controlCmp, position);
                break;
            case NavigationType::SIMPLE_LINEAR:
                if (updateComponentSimpleLinear(entity, navCmp, controlCmp, position)) {
                    onPathingFinished(navCmp, controlCmp, true /*success*/);
				}
                break;
            default:
				continue;
		}
		static_assert((int)NavigationType::INVALID == 3, "Update for new nav");
	}
}

void NavigationComponent::setSimpleLinearTargetPoint(f32v3 targetPoint) {
    mNavigationType = NavigationType::SIMPLE_LINEAR;
	mTargetPosition = targetPoint;
	mStatus = NavigationStatus::IN_PROGRESS;

    mCoarsePath.reset();
	mFinePath.reset();

	if (sDebugOptions.mShowPaths) {
		DebugRenderer::drawWireQuadThreadSafe(targetPoint, f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 0.8f), 50);
	}
}

NavPathID NavigationComponent::requestCoarsePath(f32v3 start, f32v3 goal) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
	mTargetPosition = goal;
    mCoarsePath = std::shared_ptr<NavPath>(new NavPath());
    assert(Services::isUsingNav());
	mStatus = NavigationStatus::IN_PROGRESS;
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0; // Always skip ahead two coarse points for better path
    Services::NavThread::ref().addPathfindTask(mCoarsePath, start, goal, true /*isCoarse*/, nullptr);
    return incrementNavPathID();
}

NavPathID NavigationComponent::requestCoarsePathToHarvestable(f32v3 start, TileHarvestable harvestable, f32 maxDistance) {
    mNavigationType = NavigationType::COARSE_PATH;
    mFinePath.reset();
    mTargetPosition = f32v3(0.0f);
    mCoarsePath = std::shared_ptr<NavPath>(new NavPath());
    assert(Services::isUsingNav());
	mStatus = NavigationStatus::IN_PROGRESS;
    mCurrentFinePoint = 0;
    mCurrentCoarsePoint = 0; // Always skip ahead two coarse points for better path
    Services::NavThread::ref().addPathfindToHarvestableTask(mCoarsePath, start, harvestable, maxDistance, nullptr);
	return incrementNavPathID();
}

void NavigationComponent::abort(CharacterControlComponent& motionCmp) {
    motionCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
    mStatus = NavigationStatus::IN_PROGRESS;
    mTargetPosition = f32v3(0.0f);
	mFinePath.reset();
}

NavPathID NavigationComponent::incrementNavPathID() {
    ++mCurrentNavPathID;
    if (mCurrentNavPathID == INVALID_NAV_PATH_ID) mCurrentNavPathID = 1;
    return mCurrentNavPathID;
}
