#include "stdafx.h"
#include "FishingMinigame.h"

#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderManager.h"

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/ui/InputDispatcher.h>

#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include "rendering/RenderContext.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/particle/CPUParticleSystem.h"

#include "options/DebugOptions.h"

#include "math/Random.h"

// TODO: Remove
//#include "options/DebugOptions.h"

constexpr f32 BOUNDARY_RADIUS = 100.0f;
constexpr f32 FISH_RADIUS = BOUNDARY_RADIUS * 0.3f;
constexpr f32 BOUNDARY_RADIUS_SQ = SQ(BOUNDARY_RADIUS);
constexpr f32 FISH_LIFE_LOST_COOLDOWN = 0.35f;
constexpr int PLAYER_PARTICLE_COUNT = 700; //5000
constexpr f32 CHEST_RADIUS = 15.0f;

constexpr f32 END_TRANSITION_TIME = 0.75f;

constexpr ui32 MAX_UI_ELEMENTS = 10;

// TODO: TimeUtility
f32 differenceBetweenTimePointsSeconds(TimePoint a, TimePoint b) {
    return std::chrono::duration<f32>(a - b).count();
}

f32 getFishRadius(const FishDef& fishData) {
    return FISH_RADIUS * fishData.mMinigameData.mRadius;
}

f32 getAngleOffset(const f32v2 normalizedPos, const f32v2 normal) {
    return glm::acos(glm::dot(normalizedPos, normal));
}

FishingMinigame::FishingMinigame(const FishDef& fishData, OPT FishingMinigameGameThreadData* gameThreadData, std::function<void(FishingMinigameResult& result)> onFinished, BitFlags<FishingMinigameFlags> flags) :
    mFishDef(fishData),
    mPlayerRadius(BOUNDARY_RADIUS * 0.15f),
    mFishRadius(getFishRadius(fishData)),
    mOnFinished(onFinished),
    mGameThreadData(gameThreadData),
    mFlags(flags)

{
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    mPlayerPosition = f32v2(0.0f, -BOUNDARY_RADIUS + mPlayerRadius + 1);
    mChestPosition = f32v2(0.0f, BOUNDARY_RADIUS - CHEST_RADIUS - 1);

    mSpriteBatch.init();
    mArenaShader = resourceManager.getMaterialShaderManager().getMaterialShader("fishing_arena");
    mUIShader = resourceManager.getMaterialShaderManager().getMaterialShader("textured_particle_2d");

    initUIParticles();

    initPlayerParticles();
    
    if (!mFlags.isBitSet(FishingMinigameFlags::DISABLE_DEBRIS)) {
        initBlockerParticles();
    }

    initBubbleParticles();
}

FishingMinigame::~FishingMinigame()
{
}

MinigameResult FishingMinigame::updateAndRender(const f32v2 screenResolution, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();
    mCurrentScreenResolution = screenResolution;

    mFishRadius = getFishRadius(mFishDef);

    FishingMinigameResult result;
    result.result = update();

    // Optionally update game thread data
    if (mGameThreadData) {
        std::lock_guard lock(mGameThreadData->mMutex);
        mGameThreadData->mTugOfWarValue = mTugOfWarValue;
        mGameThreadData->mBobberOffset = f32v2(mFishPosition.x, mFishPosition.y) / BOUNDARY_RADIUS;
    }

    render(elapsedSec);

    if (result.result != MinigameResultType::InProgress && mOnFinished) {
        mOnFinished(result);
        mOnFinished = nullptr;
    }
    /*if (result.result == MinigameResultType::Success) {
        assert(false);
    }
    if (result.result == MinigameResultType::Fail) {
        assert(false);
    }*/

    // Return that we caught a chest
    MinigameResult minigameResult = {
        .mType = result.result,
        .mExtraResultData = mCaughtChest ? (int)mChestTier : 0
    };
    return minigameResult;
}

void FishingMinigame::abort() {
    mStatus = MinigameResultType::Fail;

    if (mOnFinished) {
        FishingMinigameResult result;
        result.result = MinigameResultType::Fail;
        mOnFinished(result);
        mOnFinished = nullptr;
    }
}

MinigameResultType FishingMinigame::update() {

    mTickingTimer.startFrame();
    if (mStatus == MinigameResultType::InProgress) {
        if (!mTickingTimer.tryTick()) {
            return MinigameResultType::InProgress;
        }

        updatePlayerPosition(mTickingTimer.getSecPerTick());
        updateFishPosition(mTickingTimer.getSecPerTick());
        updateChestPosition(mTickingTimer.getSecPerTick());
    }

    // End Transition
    if (differenceBetweenTimePointsSeconds(mTickingTimer.getCurrTime(), mEndTransitionTimeStart) < END_TRANSITION_TIME) {
        return MinigameResultType::InProgress;
    }

    return mStatus;
}

f32v2 getBoundarySize(const f32v2 screenResolution) { return f32v2(screenResolution.y); }

// ONLY USED FOR SPRITEFONT
f32v2 getTextScreenPosition(f32v2 gamePosition, const f32v2 screenResolution) {
    const f32v2 boundarySize = getBoundarySize(screenResolution);
    const f32 screenScale = boundarySize.y / (BOUNDARY_RADIUS * 2.0);
    return gamePosition * screenScale + screenResolution * 0.5f; // Offset to center since game origin in center
}

void FishingMinigame::render(f32 elapsedSec) {
    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    const FishingMinigameFishData& minigameData = mFishDef.mMinigameData;

    constexpr f32 BOUNDARY_RADIUS_SIZE_RATIO = 1.0f / 0.527f; ////(sDebugOptions.mDebugFloat02 ? sDebugOptions.mDebugFloat02 : 1.0f);
    const f32v2 arenaSize = f32v2(BOUNDARY_RADIUS * 2.0f * BOUNDARY_RADIUS_SIZE_RATIO);

    const f32 aspectRatio = mCurrentScreenResolution.x / mCurrentScreenResolution.y;
    // Scaled transform
    const f32v2 boundarySize = getBoundarySize(mCurrentScreenResolution);
    const f32 screenScale = boundarySize.y / arenaSize.y;
    // This should get us about where we want on the screen :)
    constexpr f32 XOFFSET_MAX_DISTANCE = 0.5f;
    const f32 xOffset = (aspectRatio - 1.0f) * XOFFSET_MAX_DISTANCE;
    f32m4 camera(
        screenScale * (2.0f / mCurrentScreenResolution.x), 0, 0, 0,
        0, screenScale * (-2.0f / mCurrentScreenResolution.y), 0, 0,
        0, 0, 1.0f, 0,
        -xOffset, 0, 0, 1.0f
    );

    MaterialRenderer::bindMaterialForRender(*mUIShader);
    glUniformMatrix4fv(mUIShader->getUniform("unVP"), 1, false, &camera[0][0]);

    // Arena
    mUIParticleSystem->getEmitter(0).setParticleScale(mArenaParticleID, arenaSize);
    mBackgroundParticleSystem->getEmitter(0).setGlobalParticleScale(arenaSize * 0.6f);

    // Player
    // TODO: Replace
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();
    MaterialID materials[8] = {
        materialRepository.getMaterialDesc("soft_particle").id,
        materialRepository.getMaterialDesc("particle_v0").id,
        materialRepository.getMaterialDesc("particle_v1").id,
        materialRepository.getMaterialDesc("particle_v2").id,
        materialRepository.getMaterialDesc("particle_v3").id,
        materialRepository.getMaterialDesc("particle_v4").id,
        materialRepository.getMaterialDesc("particle_v5").id,
        materialRepository.getMaterialDesc("particle_v6").id,
    };
    // TODO: REMOVE

    mPlayerParticleSystem->getEmitter(0).setGlobalParticleScale(f32v2(5.0f, 10.0f)* f32v2(minigameData.mParticleScale));
    mUIParticleSystem->getEmitter(0).setParticleScale(mPlayerParticleID, f32v2(mPlayerRadius * 2.0f));
    mUIParticleSystem->getEmitter(0).setParticlePosition(mPlayerParticleID, f32v3(mPlayerPosition.x, mPlayerPosition.y, 0.0f));

    // Fish
    f32v4 newColor = MathUtil::lerpWithDeltaTime(mUIParticleSystem->getEmitter(0).getParticleHDRColor(mFishParticleID), f32v4(1.0f), 0.99f, elapsedSec);
    mUIParticleSystem->getEmitter(0).setParticleHDRColor(mFishParticleID, newColor);
    mUIParticleSystem->getEmitter(0).setParticleScale(mFishParticleID, f32v2(mFishRadius * 2.0f));
    mUIParticleSystem->getEmitter(0).setParticlePosition(mFishParticleID, f32v3(mFishPosition.x, mFishPosition.y, 0.0f));
    
    // Chest
    if (mChestParticleID != INVALID_PARTICLE_ID) {
        mUIParticleSystem->getEmitter(0).setParticlePosition(mChestParticleID, f32v3(mChestPosition.x, mChestPosition.y, 0.0f));
    }

    mBackgroundParticleSystem->updateAndRender(elapsedSec);
    mBubbleParticleSystem->updateAndRender(elapsedSec);
    mUIParticleSystem->updateAndRender(elapsedSec);
    if (mBlockerParticleSystem) {
        mBlockerParticleSystem->updateAndRender(elapsedSec);
    }
    mPlayerParticleSystem->updateAndRender(elapsedSec);

    vg::DepthState::restorePrevious();
}

void FishingMinigame::initUIParticles() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    mBackgroundParticleSystem = std::make_unique<CPUParticleSystem>(nullptr, 1, BitFlags<ParticleComponentType>(), *mUIShader);

    mUIParticleSystem = std::make_unique<CPUParticleSystem>(nullptr, MAX_UI_ELEMENTS,
        BitFlags<ParticleComponentType>(
            ParticleComponentType::Scale,
            ParticleComponentType::MaterialID,
            ParticleComponentType::HDRColor
        ),
        *mUIShader
    );

    constexpr const char* const possibleFishTokens[4] = {
        "fish_token_01",
        "fish_token_02",
        "fish_token_03",
        "fish_token_04",
    };

    mBackgroundParticleSystem->getEmitter(0).tryAddParticle(f32v3(0.0f));
    mBackgroundParticleSystem->getEmitter(0).setGlobalMaterialID(materialRepository.getMaterialDesc("fishing_bg").id);

    mArenaParticleID = mUIParticleSystem->getEmitter(0).tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->getEmitter(0).setParticleMaterial(mArenaParticleID, materialRepository.getMaterialDesc("fishing_border").id);
    mUIParticleSystem->getEmitter(0).setParticleHDRColor(mArenaParticleID, f32v4(1.0f));

    mFishParticleID = mUIParticleSystem->getEmitter(0).tryAddParticle(f32v3(0.0f));
    const ui32 fishTier = Random::xorshf96() % 4;
    mUIParticleSystem->getEmitter(0).setParticleMaterial(mFishParticleID, materialRepository.getMaterialDesc(possibleFishTokens[fishTier]).id);
    mUIParticleSystem->getEmitter(0).setParticleHDRColor(mFishParticleID, f32v4(1.0f));

    mPlayerParticleID = mUIParticleSystem->getEmitter(0).tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->getEmitter(0).setParticleMaterial(mPlayerParticleID, materialRepository.getMaterialDesc("fish_player").id);
    mUIParticleSystem->getEmitter(0).setParticleHDRColor(mPlayerParticleID, f32v4(0.0f, 33.5f, 0.0f, 1.0f));

    // One in 15 chance
    constexpr ui32 CHEST_FREQUENCY = 1; // 15
    const bool hasChest = (Random::getCachedRandom() % CHEST_FREQUENCY) == 0 && !mFlags.isBitSet(FishingMinigameFlags::DISABLE_CHESTS);
    if (hasChest) {
        constexpr const char* const possibleChestTokens[4] = {
            "chest_token_01",
            "chest_token_02",
            "chest_token_03",
            "chest_token_04",
        };

        const ui32 chestTier = Random::xorshf96() % 4;
        mChestParticleID = mUIParticleSystem->getEmitter(0).tryAddParticle(f32v3(0.0f));
        mUIParticleSystem->getEmitter(0).setParticleScale(mChestParticleID, f32v2(CHEST_RADIUS * 2.0f));
        mUIParticleSystem->getEmitter(0).setParticleMaterial(mChestParticleID, materialRepository.getMaterialDesc(possibleChestTokens[chestTier]).id);
        mUIParticleSystem->getEmitter(0).setParticleHDRColor(mChestParticleID, f32v4(1.0f));
    }
}

void FishingMinigame::initPlayerParticles() {

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    // Player particles
    mPlayerParticleSystem = std::make_unique<CPUParticleSystem>(
        [this](CpuParticleEmitter& emitter, CPUParticlesData& particleData, f32 elapsedSec) {

        const color4 baseColor = color4(255, 150, 92, 5);
        constexpr f32 baseScale = 0.5f;

        // Additional explosion when hitting bottom
        if (mDidPlayerImpactBottom) {
            for (ui32 i = emitter.getFirstActiveParticle(); i <= emitter.getLastActiveParticle(); ++i) {
                constexpr f32 EXPLODE_IMPULSE = 300.0f;
                f32v2 explodeDir(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f);
                explodeDir = glm::normalize(explodeDir) * Random::getCachedRandomf();
                f32v3& velocity = particleData.mVelocities[i];
                velocity.x += explodeDir.x * EXPLODE_IMPULSE;
                velocity.y += explodeDir.y * EXPLODE_IMPULSE;
            }
            mDidPlayerImpactBottom = false;
        }

        for (ui32 i = emitter.getFirstActiveParticle(); i <= emitter.getLastActiveParticle(); ++i) {
            // Check for dead particle
            if (particleData.mPositions[i].x == FLT_MAX) {
                continue;
            }
            /*  particleData.mPositions[i].x += (Random::getCachedRandomf() * 2.0f - 1.0f) * elapsedSec * 60.0f;
              particleData.mPositions[i].y += (Random::getCachedRandomf() * 2.0f - 1.0f) * elapsedSec * 60.0f;*/

            f32v3& position = particleData.mPositions[i];
            f32v3& velocity = particleData.mVelocities[i];
            f32v2& scale = particleData.mScales[i];

            const f32v2 offsetToPlayerCenter = mPlayerPosition - f32v2(position);
            const f32 distanceToPlayerCenterSQ = glm::length2(offsetToPlayerCenter);
            const f32 distanceToPlayerCenter = sqrt(distanceToPlayerCenterSQ);
            const f32v2 normalToPlayerCenter = offsetToPlayerCenter / distanceToPlayerCenter;

            f32v2 equilibriumOffset = f32v2(Random::getThreadSafef(i, 0) * 2.0f - 1.0f, Random::getThreadSafef(i, 15243) * 2.0f - 1.0f);
            equilibriumOffset = glm::normalize(equilibriumOffset) * mPlayerRadius * Random::getThreadSafef(i, 2364789);

            const auto updateCollision = [&](f32v2 targetPos, f32 targetRadius) -> bool {
                const f32v2 fishOffset = targetPos - f32v2(particleData.mPositions[i]);
                const f32 fishOffsetDistSq = glm::length2(fishOffset);
                const f32 fishOffsetDist = sqrt(fishOffsetDistSq);
                const f32 additionalFishMagnetismDist = mPlayerRadius * 0.75f;
                if (fishOffsetDist <= targetRadius + additionalFishMagnetismDist + 0.01f) {

                    constexpr f32 PULL_EXPONENT = 1.0f;
                    // Higher this is, closer we are
                    const f32 linearPushAlpha = glm::clamp(1.0f - (fishOffsetDist - targetRadius) / additionalFishMagnetismDist, 0.0f, 1.0f);
                    const f32 pushAlpha = pow(linearPushAlpha, 8.0f);

                    equilibriumOffset *= 1.0f + pushAlpha;
                    //particleData.mScales[i].y = 1.0f + pushAlpha;

                    // coalesce more on the edge
                    const f32v2 offsetFromFishCenter = (mPlayerPosition + equilibriumOffset) - targetPos;
                    const f32 distanceToFishCenter = glm::length(offsetFromFishCenter);
                    const f32v2 normalFromFishCenter = offsetFromFishCenter / distanceToFishCenter;
                    if (distanceToFishCenter <= targetRadius) {
                        // This kinda pushes an inner donut outward
                        const f32 innerDistancePower = (1.0f - (distanceToFishCenter / targetRadius));
                        const f32 outerDistancePower = (distanceToFishCenter / targetRadius);
                        const f32 distancePower = innerDistancePower * outerDistancePower * 2.0f;
                        equilibriumOffset += normalFromFishCenter * pow(distancePower, 1.0f) * targetRadius;
                    }
                    else {
                        // Beyond, pull in
                        const f32 distancePower = glm::min((distanceToFishCenter - targetRadius) / targetRadius, 1.0f);
                        equilibriumOffset -= normalFromFishCenter * distancePower * targetRadius;
                    }

                    // Color becomes warmer away from fish
                    const f32 pushColorIntensity = linearPushAlpha * glm::clamp(velocity.y * -0.05f, 0.0f, 1.0f);
                    particleData.mColors[i] = lerp(baseColor, color::White, pushColorIntensity);

                    // Scale shrinks away from fish
                    scale = f32v2(baseScale + linearPushAlpha * 0.5f);
                    // Attached to player less intensely when on fish
                    const f32 attachIntensity = 1.0f - linearPushAlpha;
                    position += f32v3(mPlayerVelocity.x * elapsedSec * attachIntensity, mPlayerVelocity.y * elapsedSec * attachIntensity, 0.0f);
                    return true;
                }
                else {
                    // Attached to player 
                    position += f32v3(mPlayerVelocity.x * elapsedSec, mPlayerVelocity.y * elapsedSec, 0.0f);
                    particleData.mColors[i] = baseColor;
                    scale = f32v2(baseScale);
                }
                return false;
            };

            // Check collision with fish and chest
            if (!updateCollision(mFishPosition, mFishRadius) && mChestParticleID != INVALID_PARTICLE_ID) {
                updateCollision(mChestPosition, CHEST_RADIUS);
            }

            // Get our equilibrium offset for the player
            f32v2 equilibriumPoint = mPlayerPosition + equilibriumOffset;
            const f32v2 offsetToEquilibriumPoint = equilibriumPoint - f32v2(position);
            const f32 distanceToEqulibriumPoint = glm::length(offsetToEquilibriumPoint);

            // Additional spring force to keep it in the bubble
            // attach particles via a spring with hookes law
            constexpr f32 SPRING_CONSTANT = 160.0f;
            // Calculate the force using Hooke's Law
            const f32v2 springForce = offsetToEquilibriumPoint * SPRING_CONSTANT;
            velocity.x += springForce.x * elapsedSec;
            velocity.y += springForce.y * elapsedSec;
            // Additional spiral force?
            f32v2 rotated = MathUtil::RotateVector(velocity.x, velocity.y, (Random::getThreadSafef(i, 25231) * 2.0f - 1.0f) * glm::min(distanceToPlayerCenter, 10.0f) * elapsedSec * 30.0f);
            velocity.x = rotated.x;
            velocity.y = rotated.y;

            // Drag when going very fast
            //if (glm::length2(velocity) > SQ(300.0f) && distanceToPlayerCenter <= mPlayerRadius) {
            //    //https://www.reddit.com/r/Unity3D/comments/5qla41/frame_rate_independent_drag/
            //    // TODO: Move out of loop
            //    velocity *= MathUtil::dragForceWithDeltaTime(0.99f, elapsedSec);
            //    particleData.mColors[i] = color::Cyan;
            //}
            //else
            if (glm::length2(velocity) > SQ(300.0f)) {
                velocity *= MathUtil::dragForceWithDeltaTime(0.99f, elapsedSec);
            }

            position += velocity * elapsedSec;


            // If too far from the equilibriumPoint, begin lerping us directly towards it
            if (distanceToEqulibriumPoint > mPlayerRadius * 0.3f) {
                f32v2 position2D = MathUtil::lerpWithDeltaTime(f32v2(position), equilibriumPoint, 0.99f, elapsedSec);
                position.x = position2D.x;
                position.y = position2D.y;
            }
        }
    },
    PLAYER_PARTICLE_COUNT,
    BitFlags<ParticleComponentType>(
        ParticleComponentType::Color,
        ParticleComponentType::Velocity,
        ParticleComponentType::Scale,
        ParticleComponentType::MaterialID
    ),
    *mUIShader);
    mPlayerParticleSystem->getEmitter(0).setGlobalParticleScale(f32v2(5.0f));

    constexpr int MATERIAL_COUNT = 9;
    MaterialID materials[MATERIAL_COUNT] = {
        materialRepository.getMaterialDesc("particle_v0").id,
        materialRepository.getMaterialDesc("particle_v1").id,
        materialRepository.getMaterialDesc("particle_v2").id,
        materialRepository.getMaterialDesc("particle_v3").id,
        materialRepository.getMaterialDesc("particle_v4").id,
        // Dominant proportion on purpose:
        materialRepository.getMaterialDesc("particle_v2").id,
        materialRepository.getMaterialDesc("particle_v2").id,
        materialRepository.getMaterialDesc("particle_v2").id,
        materialRepository.getMaterialDesc("particle_v2").id,
    };

    const f32 BALL_RADIUS = mPlayerRadius;
    for (int i = 0; i < PLAYER_PARTICLE_COUNT; ++i) {
        f32v2 randomPos(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f);
        randomPos = glm::normalize(randomPos) * Random::getCachedRandomf() * BALL_RADIUS;
        randomPos.x += mPlayerPosition.x;
        randomPos.y += mPlayerPosition.y;
        ParticleID newParticle = mPlayerParticleSystem->getEmitter(0).tryAddParticle(f32v3(
            randomPos.x,
            randomPos.y,
            0.0f)
        );
        constexpr f32 RANDOM_VEL_FORCE = 30.0f;
        mPlayerParticleSystem->getEmitter(0).setParticleVelocity(
            newParticle,
            f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 0.0f) * RANDOM_VEL_FORCE
        );
        mPlayerParticleSystem->getEmitter(0).setParticleMaterial(newParticle, materials[Random::xorshf96() % MATERIAL_COUNT]);
    }
}

void FishingMinigame::initBlockerParticles() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    // Blocker particles
    constexpr int BLOCKER_PARTICLE_COUNT = 1;
    if (BLOCKER_PARTICLE_COUNT) {
        mBlockerParticleSystem = std::make_unique<CPUParticleSystem>(
            [this](CpuParticleEmitter& emitter, CPUParticlesData& particleData, f32 elapsedSec) {

            for (ui32 i = emitter.getFirstActiveParticle(); i <= emitter.getLastActiveParticle(); ++i) {
                if (particleData.mPositions[i].x == FLT_MAX) {
                    continue;
                }

                f32v3& position = particleData.mPositions[i];
                f32v3& velocity = particleData.mVelocities[i];
                f32v2& scale = particleData.mScales[i];
                position += velocity * elapsedSec;
                const f32 distFromCenterSq = glm::length2(position);
                const f32 radius = scale.x * 0.5f;
                // Border collision
                if (distFromCenterSq > SQ(BOUNDARY_RADIUS - radius)) {
                    constexpr f32 HIT_DAMPING = 0.5f;
                    const f32v2 hitNormal = -(position / sqrt(distFromCenterSq));
                    f32v2 velocity2D(velocity);
                    velocity2D = glm::reflect(velocity2D, hitNormal) * mFishDef.mMinigameData.mWallBouncyness;
                    position.x = -hitNormal.x * (BOUNDARY_RADIUS - radius);
                    position.y = -hitNormal.y * (BOUNDARY_RADIUS - radius);
                    velocity2D *= HIT_DAMPING;
                    velocity.x = velocity2D.x;
                    velocity.y = velocity2D.y;
                }

                constexpr auto collisionCheck = [](f32v2& position, f32v2& velocity, f32v2 scale, f32v2& otherPosition, f32v2& otherVelocity, f32v2 otherScale) {
                    constexpr f32 COLLISION_ELASTICITY = 1.0f;
                    // Compute distance between the two particles
                    f32v2 diff = otherPosition - position;
                    f32 dist = glm::length(diff);
                    const f32 radius = scale.x * 0.5f;
                    const f32 otherRadius = otherScale.x * 0.5f;

                    // Check if a collision is happening
                    if (dist < radius + otherRadius) {
                        const f32 mass1 = M_PIF * SQ(radius);
                        const f32 mass2 = M_PIF * SQ(otherRadius);
                        const f32 totalMass = mass1 + mass2;
                        const f32 pushAlpha = (mass1 / totalMass);

                        // Compute unit vector in the direction of the collision
                        f32v2 unitVector = diff / dist;
                        const f32 collisionDepth = radius + otherRadius - dist;

                        // https://www.youtube.com/watch?v=WG3Sl3m4rNs&list=PLSPw4ASQYyymu3PfG9gxywSPghnSMiOAW&index=48 :3
                        const float aci = glm::dot(velocity, unitVector);
                        const float bci = glm::dot(otherVelocity, unitVector);

                        const float acf = (aci * (mass1 - mass2) + 2 * mass2 * bci) / totalMass;
                        const float bcf = (bci * (mass2 - mass1) + 2 * mass1 * aci) / totalMass;

                        velocity += (acf - aci) * unitVector;
                        otherVelocity += (bcf - bci) * unitVector;

                        // Apply inelastic collision by scaling velocities
                        velocity *= COLLISION_ELASTICITY;
                        otherVelocity *= COLLISION_ELASTICITY;

                        // Push away by collision depth
                        position -= unitVector * collisionDepth * (1.0f - pushAlpha);
                        otherPosition += unitVector * collisionDepth * pushAlpha;
                    }
                };

                // Collide with other particles
                for (int j = i + 1; j <= emitter.getLastActiveParticle(); ++j) {
                    if (particleData.mPositions[j].x == FLT_MAX) {
                        continue;
                    }

                    f32v3& otherPosition = particleData.mPositions[j];
                    f32v3& otherVelocity = particleData.mVelocities[j];
                    f32v2& otherScale = particleData.mScales[j];

                    collisionCheck((f32v2&)position, (f32v2&)velocity, scale, (f32v2&)otherPosition, (f32v2&)otherVelocity, otherScale);
                }

                // Collide with player and fish
                collisionCheck((f32v2&)position, (f32v2&)velocity, scale, mPlayerPosition, mPlayerVelocity, f32v2(mPlayerRadius * 2.0f));
                //collisionCheck((f32v2&)position, (f32v2&)velocity, scale, mFishPosition, mFishVelocity, f32v2(mFishRadius * 2.0f));
            }
        },
            BLOCKER_PARTICLE_COUNT,
            BitFlags<ParticleComponentType>(
                ParticleComponentType::Velocity,
                ParticleComponentType::Scale
            ),
            *mUIShader
        );
        mBlockerParticles.resize(BLOCKER_PARTICLE_COUNT);
        for (int i = 0; i < BLOCKER_PARTICLE_COUNT; ++i) {
            constexpr f32 PARTICLE_SCALE = 25.0f;
            f32v2 randomPos(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf()); // Spawn on bottom half always
            randomPos = glm::normalize(randomPos) * (Random::getCachedRandomf() * BOUNDARY_RADIUS + mFishRadius + PARTICLE_SCALE * 0.5f);
            mBlockerParticles[i] = mBlockerParticleSystem->getEmitter(0).tryAddParticle(f32v3(
                randomPos.x,
                randomPos.y,
                0.0f)
            );
            constexpr f32 RANDOM_VEL_FORCE = 30.0f;
            mBlockerParticleSystem->getEmitter(0).setParticleVelocity(
                mBlockerParticles[i],
                f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 0.0f) * RANDOM_VEL_FORCE
            );
            mBlockerParticleSystem->getEmitter(0).setParticleScale(mBlockerParticles[i], f32v2(PARTICLE_SCALE));
        }
        mBlockerParticleSystem->getEmitter(0).setGlobalMaterialID(materialRepository.getMaterialDesc("weed_token_01").id);
        mBlockerParticleSystem->getEmitter(0).setGlobalParticleColor(color::White);
    }
}

void FishingMinigame::initBubbleParticles()
{
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    // Blocker particles
    constexpr int BUBBLE_PARTICLE_COUNT = 40;
    if (BUBBLE_PARTICLE_COUNT) {
        mBubbleParticleSystem = std::make_unique<CPUParticleSystem>(
            [this](CpuParticleEmitter& emitter, CPUParticlesData& particleData, f32 elapsedSec) {

            constexpr f32 MAX_XVEL = 1.6f;
            constexpr f32 RANDOM_MOVE_POWER = 7.5f;
            for (ui32 i = emitter.getFirstActiveParticle(); i <= emitter.getLastActiveParticle(); ++i) {
                if (particleData.mPositions[i].x == FLT_MAX) {
                    continue;
                }

                f32v3& position = particleData.mPositions[i];
                f32v3& velocity = particleData.mVelocities[i];
                f32v2& scale = particleData.mScales[i];

                velocity.x += (Random::getCachedRandomf() * 2.0f - 1.0f) * RANDOM_MOVE_POWER;
                if (abs(velocity.x) > MAX_XVEL) {
                    velocity.x *= MathUtil::dragForceWithDeltaTime(0.99f, elapsedSec);
                }
                velocity.y = -1.0f - scale.x;
                position += velocity * elapsedSec;
                const f32 distFromCenterSq = glm::length2(position);
                const f32 radius = scale.x * 0.5f;
                // Border collision (Slightly outside border)
                if (distFromCenterSq > SQ(BOUNDARY_RADIUS + radius)) {
                    if (position.y < -BOUNDARY_RADIUS * 0.5f) {
                        // Remove this particle off top and respawn at bottom
                        position.y = -position.y;
                        f32v2 newPosition(0.0f, BOUNDARY_RADIUS + radius);
                        const f32 randomAngle = (Random::getCachedRandomf() * 2.0f - 1.0f) * DEG_TO_RAD(70.0f);
                        newPosition = MathUtil::rotateVector2DRad(newPosition, randomAngle);
                        position.x = newPosition.x;
                        position.y = newPosition.y;
                    }
                    else {
                        // Fully elastic collision
                        const f32v2 hitNormal = -(position / sqrt(distFromCenterSq));
                        f32v2 velocity2D(velocity);
                        velocity2D = glm::reflect(velocity2D, hitNormal) * mFishDef.mMinigameData.mWallBouncyness;
                        position.x = -hitNormal.x * (BOUNDARY_RADIUS + radius);
                        position.y = -hitNormal.y * (BOUNDARY_RADIUS + radius);
                        velocity.x = velocity2D.x;
                        velocity.y = velocity2D.y;
                    }
                }
            }
        },
            BUBBLE_PARTICLE_COUNT,
            BitFlags<ParticleComponentType>(
                ParticleComponentType::Velocity,
                ParticleComponentType::Scale
                ),
            *mUIShader
            );
        constexpr f32 PARTICLE_SCALE_MIN = 2.0f;
        constexpr f32 PARTICLE_SCALE_MAX = 10.0f;
        for (int i = 0; i < BUBBLE_PARTICLE_COUNT; ++i) {
            f32v2 randomPos(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f);
            randomPos = glm::normalize(randomPos) * (Random::getCachedRandomf() * BOUNDARY_RADIUS);
            ParticleID newParticle = mBubbleParticleSystem->getEmitter(0).tryAddParticle(f32v3(
                randomPos.x,
                randomPos.y,
                0.0f)
            );
            constexpr f32 RANDOM_VEL_FORCE = 30.0f;
            mBubbleParticleSystem->getEmitter(0).setParticleVelocity(
                newParticle,
                f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 0.0f) * RANDOM_VEL_FORCE
            );
            f32 scale = Random::getCachedRandomf();
            scale = pow(scale, 8.0f); // Favor smaller bubbles
            scale = scale * (PARTICLE_SCALE_MAX - PARTICLE_SCALE_MIN) + PARTICLE_SCALE_MIN;
            mBubbleParticleSystem->getEmitter(0).setParticleScale(newParticle, f32v2(scale));
        }
        mBubbleParticleSystem->getEmitter(0).setGlobalMaterialID(materialRepository.getMaterialDesc("fish_bubble").id);
        mBubbleParticleSystem->getEmitter(0).setGlobalParticleColor(color::White);
    }
}

f32v2 getRandomDirectionVector() {
    // TOOD: Fishing internal stable random
    return glm::normalize(f32v2(Random::xorshf96f() * 2.0f - 1.0f, Random::xorshf96f() * 2.0f - 1.0f));
}

void FishingMinigame::updateFishPosition(f32 elapsedSec) {

    const FishingMinigameFishData& minigameData = mFishDef.mMinigameData;
    const TimePoint currentTime = mTickingTimer.getCurrTime();

    // Framerate independant acceleration https://stackoverflow.com/questions/43960217/framerate-independent-acceleration-decceleration
    const f32v2 accelerationForce = getRandomDirectionVector() * minigameData.mAcceleration;
    MathUtil::accelerateWithDeltaTime(mFishPosition, mFishVelocity, accelerationForce, elapsedSec);

    //  Drag
    mFishVelocity *= MathUtil::dragForceWithDeltaTime(minigameData.mFishDrag, elapsedSec);

    mFishPosition += mFishVelocity * elapsedSec;

    const f32 fishDist = glm::length(mFishPosition);
    const f32v2 fishNormal = mFishPosition / fishDist;

    // Jerk
    if (Random::xorshf96f() <= minigameData.mJerkChance) {
        if (differenceBetweenTimePointsSeconds(currentTime, mLastJerkTime) >= mCurrentJerkCooldown) {
            addDebugFloater("JERK", getTextScreenPosition(mFishPosition, mCurrentScreenResolution), color::White);
            mLastJerkTime = currentTime;
            mFishVelocity += getRandomDirectionVector() * minigameData.mJerkIntensity;
            mCurrentJerkCooldown = lerp(Random::xorshf96f(), minigameData.mJerkCooldownVarianceSec.x, minigameData.mJerkCooldownVarianceSec.y);
        }
    }

    // Center magnitism
    mFishVelocity += -fishNormal * minigameData.mCenterMagnitism * elapsedSec;

    // Gravity
    mFishVelocity.y += minigameData.mGravity * elapsedSec;

    // Player collision
    // Dont collide if we just took a fish life
    if (differenceBetweenTimePointsSeconds(currentTime, mLastFishLifeLostTime) >= FISH_LIFE_LOST_COOLDOWN) {
        const f32v2 offsetToPlayer = mPlayerPosition - mFishPosition;
        const f32 distanceFromPlayer = glm::length(offsetToPlayer);
        const f32 totalRadius = mPlayerRadius + mFishRadius;
        if (distanceFromPlayer <= totalRadius) {
            const f32v2 normalToPlayer = offsetToPlayer / distanceFromPlayer;
            const f32 overlapAmount = totalRadius - distanceFromPlayer;
            const f32 overlapPower = pow(glm::min(overlapAmount / mFishRadius, 1.0f), 0.7f);
            // TODO: Use elapsed?
            const f32v2 lerpAlpha = minigameData.mPlayerStickyness * overlapPower;
            f32v2 framerateIndependantLerpAlpha;
            mFishVelocity.x = MathUtil::lerpWithDeltaTime(mFishVelocity.x, 0.0f, lerpAlpha.x, elapsedSec);
            mFishVelocity.y = MathUtil::lerpWithDeltaTime(mFishVelocity.y, -minigameData.mPlayerStrength * overlapPower, lerpAlpha.y, elapsedSec);
            mIsPlayerTouchingFish = true;
        }
        else {
            mIsPlayerTouchingFish = false;
        }
    }

    const f32 fishVelocitySq = glm::length2(mFishVelocity);
    if (fishVelocitySq > SQ(minigameData.mMaxSpeed)) {
        // Clamp velocity
        mFishVelocity = (mFishVelocity / sqrt(fishVelocitySq)) * minigameData.mMaxSpeed;
    }

    // Collide
    if (fishDist + mFishRadius > BOUNDARY_RADIUS) {
        mFishVelocity = glm::reflect(mFishVelocity, -fishNormal) * minigameData.mWallBouncyness;
        mFishPosition = fishNormal * (BOUNDARY_RADIUS - mFishRadius);
        // Success / fail
        if (getAngleOffset(fishNormal, f32v2(0.0f, -1.0f)) <= DEG_TO_RAD(minigameData.mSuccessAngle)) {
            fishLifeLost();
        }
        else if (getAngleOffset(fishNormal, f32v2(0.0f, 1.0f)) <= DEG_TO_RAD(minigameData.mFailAngle)) {
            playerLifeLost();
        }
    }
}

void FishingMinigame::updatePlayerPosition(f32 elapsedSec) {

    const FishingMinigameFishData& minigameData = mFishDef.mMinigameData;

    f32v2 inputDir(0.0f);
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_W) || vui::InputDispatcher::key.isKeyPressed(VKEY_UP)) {
        inputDir.y = -1.0f;
    }
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_S) || vui::InputDispatcher::key.isKeyPressed(VKEY_DOWN)) {
        inputDir.y = 1.0f;
    }
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_A) || vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT)) {
        inputDir.x = -1.0f;
    }
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_D) || vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT)) {
        inputDir.x = 1.0f;
    }
    if (inputDir != f32v2(0.0f)) {
        inputDir = glm::normalize(inputDir);
    }

    // TODO: Shared with fish

     // Framerate independant acceleration https://stackoverflow.com/questions/43960217/framerate-independent-acceleration-decceleration
    const f32v2 acceleration = inputDir * minigameData.mPlayerAcceleration;
    MathUtil::accelerateWithDeltaTime(mPlayerPosition, mPlayerVelocity, acceleration, elapsedSec);

    // Drag
    mPlayerVelocity *= MathUtil::dragForceWithDeltaTime(minigameData.mPlayerDrag, elapsedSec);

    // TMP SIMPLE VELOCITY
    //mPlayerVelocity = minigameData.mPlayerAcceleration * 2.0f * inputDir;


    const f32 playerVelocitySq = glm::length2(mPlayerVelocity);
    if (playerVelocitySq >= SQ(minigameData.mPlayerMaxSpeed)) {
        // Clamp velocity
        mPlayerVelocity = (mPlayerVelocity / sqrt(playerVelocitySq)) * minigameData.mPlayerMaxSpeed;
    }

    const f32 playerDist = glm::length(mPlayerPosition);
    const f32v2 playerNormal = mPlayerPosition / playerDist;

    // Collide
    if (playerDist + mPlayerRadius > BOUNDARY_RADIUS) {
        mPlayerVelocity = glm::reflect(mPlayerVelocity, -playerNormal) * minigameData.mPlayerWallBouncyness;
        mPlayerPosition = playerNormal * (BOUNDARY_RADIUS - mPlayerRadius);

        if (mIsPlayerTouchingFish && getAngleOffset(playerNormal, f32v2(0.0f, 1.0f)) <= DEG_TO_RAD(minigameData.mSuccessAngle)) {
            fishLifeLost();
        }
    }
}

void FishingMinigame::updateChestPosition(f32 elapsedSec) {
    if (mChestParticleID == INVALID_PARTICLE_ID) {
        return;
    }

    const FishingMinigameFishData& minigameData = mFishDef.mMinigameData;

    mChestVelocity *= MathUtil::dragForceWithDeltaTime(0.03f, elapsedSec);
    mChestPosition += mChestVelocity * elapsedSec;

    // Gravity (Half)
    mChestVelocity.y += minigameData.mGravity * elapsedSec;

    // Player collide
    const f32v2 offsetToPlayer = mPlayerPosition - mChestPosition;
    const f32 distanceFromPlayer = glm::length(offsetToPlayer);
    const f32 totalRadius = mPlayerRadius + CHEST_RADIUS;
    if (distanceFromPlayer <= totalRadius) {
        constexpr f32 CHEST_PULL_RATIO = 0.5f; // Less = slower pull vs fish
        const f32v2 normalToPlayer = offsetToPlayer / distanceFromPlayer;
        const f32 overlapAmount = totalRadius - distanceFromPlayer;
        const f32 overlapPower = pow(glm::min(overlapAmount / CHEST_RADIUS, 1.0f), 0.7f);
        // TODO: Use elapsed?
        const f32v2 lerpAlpha = minigameData.mPlayerStickyness * overlapPower;
        f32v2 framerateIndependantLerpAlpha;
        mChestVelocity.x = MathUtil::lerpWithDeltaTime(mChestVelocity.x, 0.0f, lerpAlpha.x, elapsedSec);
        mChestVelocity.y = MathUtil::lerpWithDeltaTime(mChestVelocity.y, -minigameData.mPlayerStrength * overlapPower * CHEST_PULL_RATIO, lerpAlpha.y, elapsedSec);
    }

    // Wall collide
    const f32 chestDist = glm::length(mChestPosition);
    const f32v2 chestNormal = mChestPosition / chestDist;
    if (chestDist + CHEST_RADIUS > BOUNDARY_RADIUS) {
        mChestVelocity = glm::reflect(mChestVelocity, -chestNormal) * minigameData.mWallBouncyness;
        mChestPosition = chestNormal * (BOUNDARY_RADIUS - CHEST_RADIUS);
        // Success / fail
        if (getAngleOffset(chestNormal, f32v2(0.0f, -1.0f)) <= DEG_TO_RAD(minigameData.mSuccessAngle)) {
            catchChest();
        }
    }
}

void FishingMinigame::fishLifeLost() {
    constexpr f32 FISH_LAUNCH_VEL = 20.0f;
    addDebugFloater("SUCCESS", getTextScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
    if (++mTugOfWarValue >= 3) {
        win();
        return;
    }
    mDidPlayerImpactBottom = true;
    mFishVelocity.y = FISH_LAUNCH_VEL;
    mIsPlayerTouchingFish = false;
    mLastFishLifeLostTime = mTickingTimer.getCurrTime();

    // HDR Coloring
    mUIParticleSystem->getEmitter(0).setParticleHDRColor(mFishParticleID, f32v4(1.27f, 1.725f, 1.514f, 1.0f));
}

void FishingMinigame::playerLifeLost() {
    constexpr f32 FISH_LAUNCH_VEL = 60.0f;
    addDebugFloater("FAIL", getTextScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
    if (--mTugOfWarValue <= -3) {
        lose();
        return;
    }
    mFishVelocity.y = -FISH_LAUNCH_VEL;

    // HDR Coloring
    mUIParticleSystem->getEmitter(0).setParticleHDRColor(mFishParticleID, f32v4(1.0f, 0.404f, 0.063, 1.0f));
}

void FishingMinigame::catchChest() {
    mCaughtChest = true;
    // TODO: VFX
    mUIParticleSystem->getEmitter(0).removeParticle(mChestParticleID);
    mChestParticleID = INVALID_PARTICLE_ID;
}

void FishingMinigame::win() {
    mStatus = MinigameResultType::Success;
    mEndTransitionTimeStart = mTickingTimer.getCurrTime();

    addDebugFloater("YOU WIN!", mCurrentScreenResolution * 0.5f, color::Green);
}

void FishingMinigame::lose() {
    mStatus = MinigameResultType::Fail;
    mEndTransitionTimeStart = mTickingTimer.getCurrTime();

    addDebugFloater("YOU LOSE!", mCurrentScreenResolution * 0.5f, color::Green);
}

void FishingMinigame::renderDebugFloaters() {
    constexpr f32 FLOATER_DURATION_SEC = 0.75f;
    constexpr f32 FLOATER_SIZE = 1.0f;
    for (auto&& it = mDebugFloaters.begin(); it != mDebugFloaters.end();) {
        if (differenceBetweenTimePointsSeconds(mTickingTimer.getCurrTime(), it->mSpawnTime) > FLOATER_DURATION_SEC) {
            it = mDebugFloaters.erase(it);
        }
        else {
            mSpriteBatch.drawString(
                &RenderContext::getInstance().getDebugFont(),
                it->mText,
                it->mPosition,
                f32v2(FLOATER_SIZE),
                it->mColor,
                vg::TextAlign::CENTER
            );
            ++it;
        }
    }
}

void FishingMinigame::addDebugFloater(const char* text, f32v2 position, color4 color) {
    MinigameDebugTextFloater floater;
    floater.mText = text;
    floater.mPosition = position;
    floater.mColor = color;
    floater.mSpawnTime = mTickingTimer.getCurrTime();
    mDebugFloaters.emplace_back(floater);
}
