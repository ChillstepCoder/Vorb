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
#include "rendering/particle/CPUParticleSystem2D.h"

#include "options/DebugOptions.h"

#include "math/Random.h"

// TODO: Remove
//#include "options/DebugOptions.h"

constexpr f32 BOUNDARY_RADIUS = 100.0f;
constexpr f32 FISH_RADIUS = BOUNDARY_RADIUS * 0.3f;
constexpr f32 BOUNDARY_RADIUS_SQ = SQ(BOUNDARY_RADIUS);
constexpr f32 FISH_LIFE_LOST_COOLDOWN = 0.35f;
constexpr int PLAYER_PARTICLE_COUNT = 500; //5000

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

FishingMinigame::FishingMinigame(const FishDef& fishData, std::function<void(FishingMinigameResult& result)> onFinished) :
    mFishDef(fishData),
    mPlayerRadius(BOUNDARY_RADIUS * 0.15f),
    mFishRadius(getFishRadius(fishData)),
    mOnFinished(onFinished)

{
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();


    mPlayerPosition = f32v2(0.0f, -BOUNDARY_RADIUS + mPlayerRadius + 1);
    mSpriteBatch.init();
    mArenaShader = resourceManager.getMaterialShaderManager().getMaterialShader("fishing_arena");
    mUIShader = resourceManager.getMaterialShaderManager().getMaterialShader("textured_particle_2d");

    initUIParticles();

    initPlayerParticles();
    
    initBlockerParticles();
}

FishingMinigame::~FishingMinigame()
{
}

MinigameResultType FishingMinigame::updateAndRender(const f32v2 screenResolution, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();
    mCurrentScreenResolution = screenResolution;

    mFishRadius = getFishRadius(mFishDef);

    FishingMinigameResult result;
    result.result = update();
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
    return result.result;
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
    mUIParticleSystem->setParticleScale(mArenaParticleID, arenaSize);
    mUIParticleSystem->setParticleScale(mBackgroundParticleID, arenaSize * 0.6f);

    // Player
    mUIParticleSystem->setParticleScale(mPlayerParticleID, f32v2(mPlayerRadius * 2.0f));
    mUIParticleSystem->setParticlePosition(mPlayerParticleID, f32v3(mPlayerPosition.x, mPlayerPosition.y, 0.0f));

    // Fish
    mUIParticleSystem->setParticleScale(mFishParticleID, f32v2(mFishRadius * 2.0f));
    mUIParticleSystem->setParticlePosition(mFishParticleID, f32v3(mFishPosition.x, mFishPosition.y, 0.0f));

    mUIParticleSystem->updateAndRender(mUIShader->mProgram, elapsedSec);
    mBlockerParticleSystem->updateAndRender(mUIShader->mProgram, elapsedSec);
    mPlayerParticleSystem->updateAndRender(mUIShader->mProgram, elapsedSec);

    vg::DepthState::restorePrevious();
}

void FishingMinigame::initUIParticles() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    mUIParticleSystem = std::make_unique<CPUParticleSystem2D>(nullptr, MAX_UI_ELEMENTS,
        BitFlags<ParticleComponentType>(
            ParticleComponentType::Scale,
            ParticleComponentType::MaterialID,
            ParticleComponentType::Color
        )
    );

    const char* possibleTokens[4] = {
        "fish_token_01",
        "fish_token_02",
        "fish_token_03",
        "fish_token_04",
    };

    mBackgroundParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mBackgroundParticleID, materialRepository.getMaterialDesc("fishing_bg").id);
    mUIParticleSystem->setParticleColor(mBackgroundParticleID, color::White);

    mArenaParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mArenaParticleID, materialRepository.getMaterialDesc("fishing_border").id);
    mUIParticleSystem->setParticleColor(mArenaParticleID, color::White);

    mFishParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mFishParticleID, materialRepository.getMaterialDesc(possibleTokens[Random::xorshf96() % 4]).id);
    mUIParticleSystem->setParticleColor(mFishParticleID, color::White);

    mPlayerParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mPlayerParticleID, materialRepository.getMaterialDesc("fish_player").id);
    mUIParticleSystem->setParticleColor(mPlayerParticleID, color::Green);
}

void FishingMinigame::initPlayerParticles() {

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    // Player particles
    mPlayerParticleSystem = std::make_unique<CPUParticleSystem2D>(
        [this](CPUParticleSystem2D& system, CPUParticleSystemData2D& particleData, f32 elapsedSec) {

        // Additional explosion when hitting bottom
        if (mDidPlayerImpactBottom) {
            for (ui32 i = system.getFirstActiveParticle(); i <= system.getLastActiveParticle(); ++i) {
                constexpr f32 EXPLODE_IMPULSE = 300.0f;
                f32v2 explodeDir(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f);
                explodeDir = glm::normalize(explodeDir) * Random::getCachedRandomf();
                f32v3& velocity = particleData.mVelocities[i];
                velocity.x += explodeDir.x * EXPLODE_IMPULSE;
                velocity.y += explodeDir.y * EXPLODE_IMPULSE;
            }
            mDidPlayerImpactBottom = false;
        }

        for (ui32 i = system.getFirstActiveParticle(); i <= system.getLastActiveParticle(); ++i) {
            // Check for dead particle
            if (particleData.mPositions[i].x == FLT_MAX) {
                continue;
            }
            /*  particleData.mPositions[i].x += (Random::getCachedRandomf() * 2.0f - 1.0f) * elapsedSec * 60.0f;
              particleData.mPositions[i].y += (Random::getCachedRandomf() * 2.0f - 1.0f) * elapsedSec * 60.0f;*/

            f32v3& position = particleData.mPositions[i];
            f32v3& velocity = particleData.mVelocities[i];

            const f32v2 offsetToPlayerCenter = mPlayerPosition - f32v2(position);
            const f32 distanceToPlayerCenterSQ = glm::length2(offsetToPlayerCenter);
            const f32 distanceToPlayerCenter = sqrt(distanceToPlayerCenterSQ);
            const f32v2 normalToPlayerCenter = offsetToPlayerCenter / distanceToPlayerCenter;

            //  Cool colors
            particleData.mColors[i].r = (ui8)glm::min(distanceToPlayerCenter * 10.0f, 255.0f);
            particleData.mColors[i].g = 255ui8 - (ui8)glm::min(distanceToPlayerCenter * 5.0f, 255.0f);
            particleData.mColors[i].b = (ui8)glm::min(distanceToPlayerCenter * 5.0f, 255.0f);
            particleData.mColors[i].a = 5;

            f32v2 equilibriumOffset = f32v2(Random::getThreadSafef(i, 0) * 2.0f - 1.0f, Random::getThreadSafef(i, 15243) * 2.0f - 1.0f);
            equilibriumOffset = glm::normalize(equilibriumOffset) * mPlayerRadius * Random::getThreadSafef(i, 2364789);

            {// Check collision with fish
                const f32v2 fishOffset = mFishPosition - f32v2(particleData.mPositions[i]);
                const f32 fishOffsetDistSq = glm::length2(fishOffset);
                const f32 fishOffsetDist = sqrt(fishOffsetDistSq);
                const f32 additionalFishMagnetismDist = mPlayerRadius * 0.25f;
                if (fishOffsetDist <= mFishRadius + additionalFishMagnetismDist + 0.01f) {

                    constexpr f32 PULL_EXPONENT = 1.0f;
                    const f32 pushAlpha = pow(glm::clamp(1.0f - (fishOffsetDist - mFishRadius) / additionalFishMagnetismDist, 0.0f, 1.0f), 8.0f);

                    equilibriumOffset *= 1.0f + pushAlpha;
                    particleData.mScales[i].y = 1.0f + pushAlpha;

                    // coalesce more on the edge
                    const f32v2 offsetFromFishCenter = (mPlayerPosition + equilibriumOffset) - mFishPosition;
                    const f32 distanceToFishCenter = glm::length(offsetFromFishCenter);
                    const f32v2 normalFromFishCenter = offsetFromFishCenter / distanceToFishCenter;
                    if (distanceToFishCenter <= mFishRadius) {
                        // This kinda pushes an inner donut outward
                        const f32 innerDistancePower = (1.0f - (distanceToFishCenter / mFishRadius));
                        const f32 outerDistancePower = (distanceToFishCenter / mFishRadius);
                        const f32 distancePower = innerDistancePower * outerDistancePower * 2.0f;
                        equilibriumOffset += normalFromFishCenter * pow(distancePower, 1.0f + sDebugOptions.mDebugFloat02) * mFishRadius;
                    }
                    else {
                        // Beyond, pull in
                        const f32 distancePower = glm::min((distanceToFishCenter - mFishRadius) / mFishRadius, 1.0f);
                        equilibriumOffset -= normalFromFishCenter * distancePower * mFishRadius;
                    }

                    const f32 pushColorIntensity = pow(pushAlpha, 0.1f) * glm::clamp(velocity.y * -0.05f, 0.0f, 1.0f);
                    particleData.mColors[i] = lerp(particleData.mColors[i], color4(0, 255, 0, 255), pushColorIntensity);
                }
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

            // Attached to player 
            position += f32v3(mPlayerVelocity.x * elapsedSec, mPlayerVelocity.y * elapsedSec, 0.0f);

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
            ParticleComponentType::Scale
        )
        );
    mPlayerParticleSystem->setGlobalMaterialID(materialRepository.getMaterialDesc("soft_particle").id);
    mPlayerParticleSystem->setGlobalParticleScale(f32v2(5.0f));

    const f32 BALL_RADIUS = mPlayerRadius;
    for (int i = 0; i < PLAYER_PARTICLE_COUNT; ++i) {
        f32v2 randomPos(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f);
        randomPos = glm::normalize(randomPos) * Random::getCachedRandomf() * BALL_RADIUS;
        randomPos.x += mPlayerPosition.x;
        randomPos.y += mPlayerPosition.y;
        ParticleID newParticle = mPlayerParticleSystem->tryAddParticle(f32v3(
            randomPos.x,
            randomPos.y,
            0.0f)
        );
        constexpr f32 RANDOM_VEL_FORCE = 30.0f;
        mPlayerParticleSystem->setParticleVelocity(
            newParticle,
            f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 0.0f) * RANDOM_VEL_FORCE
        );
    }
}

void FishingMinigame::initBlockerParticles() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    // Blocker particles
    constexpr int BLOCKER_PARTICLE_COUNT = 30;
    if (BLOCKER_PARTICLE_COUNT) {
        mBlockerParticleSystem = std::make_unique<CPUParticleSystem2D>(
            [this](CPUParticleSystem2D& system, CPUParticleSystemData2D& particleData, f32 elapsedSec) {

            for (ui32 i = system.getFirstActiveParticle(); i <= system.getLastActiveParticle(); ++i) {
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
                for (int j = i + 1; j <= system.getLastActiveParticle(); ++j) {
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
            )
        );
        mBlockerParticles.resize(BLOCKER_PARTICLE_COUNT);
        for (int i = 0; i < BLOCKER_PARTICLE_COUNT; ++i) {
            constexpr f32 PARTICLE_SCALE = 5.0f;
            f32v2 randomPos(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf()); // Spawn on bottom half always
            randomPos = glm::normalize(randomPos) * (Random::getCachedRandomf() * BOUNDARY_RADIUS + mFishRadius + PARTICLE_SCALE * 0.5f);
            mBlockerParticles[i] = mBlockerParticleSystem->tryAddParticle(f32v3(
                randomPos.x,
                randomPos.y,
                0.0f)
            );
            constexpr f32 RANDOM_VEL_FORCE = 30.0f;
            mBlockerParticleSystem->setParticleVelocity(
                mBlockerParticles[i],
                f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 0.0f) * RANDOM_VEL_FORCE
            );
            mBlockerParticleSystem->setParticleScale(mBlockerParticles[i], f32v2(PARTICLE_SCALE));
        }
        mBlockerParticleSystem->setGlobalMaterialID(materialRepository.getMaterialDesc("fish_bubble").id);
        mBlockerParticleSystem->setGlobalParticleColor(color::White);
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
}

void FishingMinigame::playerLifeLost() {
    constexpr f32 FISH_LAUNCH_VEL = 60.0f;
    addDebugFloater("FAIL", getTextScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
    if (--mTugOfWarValue <= -3) {
        lose();
        return;
    }
    mFishVelocity.y = -FISH_LAUNCH_VEL;
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
