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

enum class PARTICLE_TEST {
    SimpleGravity,
    FixedPosition,
    GravitySpring
};
constexpr PARTICLE_TEST PARTICLE_MODE = PARTICLE_TEST::GravitySpring;

constexpr f32 BOUNDARY_RADIUS = 100.0f;
constexpr f32 FISH_RADIUS = BOUNDARY_RADIUS * 0.3f;
constexpr f32 BOUNDARY_RADIUS_SQ = SQ(BOUNDARY_RADIUS);
constexpr f32 FISH_LIFE_LOST_COOLDOWN = 0.35f;

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

FishingMinigame::FishingMinigame(const FishDef& fishData) :
    mFishDef(fishData),
    mPlayerRadius(BOUNDARY_RADIUS * 0.1f),
    mFishRadius(getFishRadius(fishData))

{
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialRepository& materialRepository = resourceManager.getMaterialRepository();

    // UI particles
    mUIParticleSystem = std::make_unique<CPUParticleSystem2D>(nullptr, MAX_UI_ELEMENTS,
        BitFlags<ParticleComponentType>(
            ParticleComponentType::Scale,
            ParticleComponentType::MaterialID,
            ParticleComponentType::Color
        )
    );
    mPlayerPosition = f32v2(0.0f, BOUNDARY_RADIUS - mPlayerRadius - 1);
    mSpriteBatch.init();
    mArenaShader = resourceManager.getMaterialShaderManager().getMaterialShader("fishing_arena");
    mUIShader = resourceManager.getMaterialShaderManager().getMaterialShader("textured_particle_2d");

    // Set up particles
    // TODO: REMOVE 99999


    mBackgroundParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mBackgroundParticleID, materialRepository.getMaterialDesc("fishing_circle").id);
    mUIParticleSystem->setParticleScale(mBackgroundParticleID, f32v2(10000.0f));
    mUIParticleSystem->setParticleColor(mBackgroundParticleID, color::Black);

    mArenaParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f)); 
    mUIParticleSystem->setParticleMaterial(mArenaParticleID, materialRepository.getMaterialDesc("fishing_border").id);
    mUIParticleSystem->setParticleColor(mArenaParticleID, color::White);

    mFishParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mFishParticleID, materialRepository.getMaterialDesc("hard_particle").id);
    mUIParticleSystem->setParticleColor(mFishParticleID, color::Red);

    mPlayerParticleID = mUIParticleSystem->tryAddParticle(f32v3(0.0f));
    mUIParticleSystem->setParticleMaterial(mPlayerParticleID, materialRepository.getMaterialDesc("soft_particle").id);
    mUIParticleSystem->setParticleColor(mPlayerParticleID, color::Green);

    // Player particles
    constexpr int PLAYER_PARTICLE_COUNT = 5000;
    mPlayerParticleSystem = std::make_unique<CPUParticleSystem2D>(
        [this](CPUParticleSystem2D& system, CPUParticleSystemData2D& particleData, f32 elapsedSec) {
        for (ui32 i = system.getFirstActiveParticle(); i <= system.getLastActiveParticle(); ++i) {
            // Check for dead particle
            if (particleData.mPositions[i].x == FLT_MAX) {
                continue;
            }
            /*  particleData.mPositions[i].x += (Random::getCachedRandomf() * 2.0f - 1.0f) * elapsedSec * 60.0f;
              particleData.mPositions[i].y += (Random::getCachedRandomf() * 2.0f - 1.0f) * elapsedSec * 60.0f;*/

            f32v3& position = particleData.mPositions[i];
            f32v3& velocity = particleData.mVelocities[i];

            // Cool test for particle velocity
            if (PARTICLE_MODE == PARTICLE_TEST::SimpleGravity) {
                const f32v2 offsetToPlayer = f32v2(mPlayerPosition.x - position.x, mPlayerPosition.y - position.y);
                const f32 distanceToPlayer = glm::length(offsetToPlayer);
                const f32v2 normalToPlayer = offsetToPlayer / distanceToPlayer;

                constexpr f32 ACCEL = 60.0f;
                const f32 DRAG = pow(0.95f, elapsedSec); // POW makes it framerate independant
                velocity.x *= DRAG;
                velocity.y *= DRAG;
                velocity.x += normalToPlayer.x * elapsedSec * ACCEL; // * elapsedSec!!!
                velocity.y += normalToPlayer.y * elapsedSec * ACCEL;

                position += velocity * elapsedSec;

                const ui8 distCol = (ui8)glm::min(distanceToPlayer, 255.0f);
                particleData.mColors[i].r = distCol;
                particleData.mColors[i].g = 1.0 - distCol;
                particleData.mColors[i].b = 0;
            }
            else if (PARTICLE_MODE == PARTICLE_TEST::FixedPosition) {

                // Local position
                const f32 gravityForce = 60.0f * elapsedSec;
                const f32v2 offsetToPlayerCenter = -f32v2(position.x, position.y);
                const f32 distanceToPlayerCenter = glm::length(offsetToPlayerCenter);
                const f32v2 normalToPlayerCenter = offsetToPlayerCenter / distanceToPlayerCenter;
                velocity.x += normalToPlayerCenter.x * gravityForce;
                velocity.y += normalToPlayerCenter.y * gravityForce;
                position += velocity * elapsedSec;

                // World position (Parented to player)
                const f32v2 worldPos(f32v2(position) + mPlayerPosition);
                // Check collision with fish
                const f32v2 fishOffset = mFishPosition - worldPos;
                if (glm::length2(fishOffset) < SQ(mFishRadius)) {
                    particleData.mColors[i] = color::Yellow;
                }
                else {
                    particleData.mColors[i] = color::Cyan;
                }
            }
            else if (PARTICLE_MODE == PARTICLE_TEST::GravitySpring) {
                const f32 gravityForce = 120.0f * elapsedSec;
                const f32v2 offsetToPlayerCenter = mPlayerPosition - f32v2(position.x, position.y);
                const f32 distanceToPlayerCenterSQ = glm::length2(offsetToPlayerCenter);
                const f32 distanceToPlayerCenter = sqrt(distanceToPlayerCenterSQ);
                const f32v2 normalToPlayerCenter = offsetToPlayerCenter / distanceToPlayerCenter;

                // Additional spring force to keep it in the bubble
                // attach particles via a spring with hookes law
                constexpr f32 SPRING_CONSTANT = 80.0f;
                constexpr f32 SPRING_DAMPING = 0.25f;
                // Calculate the force using Hooke's Law
                const f32v2 springForce = offsetToPlayerCenter * SPRING_CONSTANT - f32v2(velocity) * SPRING_DAMPING;
              /*  if (distanceToPlayerCenter > mPlayerRadius) {
                    force += (distanceToPlayerCenter - mPlayerRadius) * SPRING_FORCE * elapsedSec;
                    particleData.mColors[i] = color::Red;
                }
                else {*/
                    particleData.mColors[i] = color::Cyan;
               // }

                velocity.x += springForce.x * elapsedSec;
                velocity.y += springForce.y * elapsedSec;

                // If too far from the player, begin lerping us directly towards him
                if (distanceToPlayerCenter > 1.5f) {
                    f32v2 lerpPos = lerp(f32v2(position), mPlayerPosition, 0.1f);
                    position.x = lerpPos.x;
                    position.y = lerpPos.y;
                }

                // Drag when close to player
                if (glm::length2(velocity) > SQ(30.0f) && distanceToPlayerCenter <= mPlayerRadius) {
                    //https://www.reddit.com/r/Unity3D/comments/5qla41/frame_rate_independent_drag/
                    // TODO: Move out of loop
                    const f32 DRAG = exp(-0.7f * elapsedSec);
                    //particleData.mColors[i] = color::Blue;
                    velocity *= DRAG;
                }

                position += velocity * elapsedSec;

                // Attached to player 
                position += f32v3(mPlayerVelocity.x, mPlayerVelocity.y, 0.0f);

                //  Cool colors
                particleData.mColors[i].r = (ui8)glm::min(distanceToPlayerCenter * 10.0f, 255.0f);
                particleData.mColors[i].g = 255ui8 - (ui8)glm::min(distanceToPlayerCenter * 5.0f, 255.0f);
                particleData.mColors[i].b = 255u - (ui8)glm::min(distanceToPlayerCenter * 5.0f, 255.0f);

                // Check collision with fish
                const f32v2 fishOffset = mFishPosition - f32v2(particleData.mPositions[i]);
                const f32 fishOffsetDistSq = glm::length2(fishOffset);
                const f32 fishOffsetDist = sqrt(fishOffsetDistSq);
                if (fishOffsetDistSq < SQ(mFishRadius)) {
                    particleData.mColors[i] = lerp(particleData.mColors[i], color::Yellow, 1.0f - fishOffsetDist);
                    // When colliding, apply brownian jitter
                    constexpr f32 BROWNIAN_FORCE = 3000.0f;
                    f32v2 brownianVec = f32v2(Random::getThreadSafef(i, 0) * 2.0f - 1.0f, Random::getThreadSafef(i, 15243) * 2.0f - 1.0f);
                    brownianVec = glm::normalize(brownianVec) * BROWNIAN_FORCE * Random::getThreadSafef(i, 2364789);
                    
                    // TODO: This creates emergent behavior! It sticks in place. WHY??!?? :O
                    velocity.x += brownianVec.x * elapsedSec;
                    velocity.y += brownianVec.y * elapsedSec;
                } else if (fishOffsetDist < mFishRadius + 1.0f) {
                    const f32 lerpFactor = pow(1.0f - (fishOffsetDist - mFishRadius), 1.0f);
                    const f32v2 fishDirFromPlayer = glm::normalize(mFishPosition - mPlayerPosition);
                    position.x += lerpFactor * fishDirFromPlayer.x;
                    position.y += lerpFactor * fishDirFromPlayer.y;
                }
            }
        }
        },
        PLAYER_PARTICLE_COUNT,
        BitFlags<ParticleComponentType>(
            ParticleComponentType::Color,
            ParticleComponentType::Velocity
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

FishingMinigame::~FishingMinigame()
{
}

FishingMinigameResult FishingMinigame::updateAndRender(const f32v2 screenResolution, f32 elapsedSec) {
    mCurrentScreenResolution = screenResolution;

    mFishRadius = getFishRadius(mFishDef);

    FishingMinigameResult result;
    result.result = update();
    render(elapsedSec);
    /*if (result.result == MinigameResultType::Success) {
        assert(false);
    }
    if (result.result == MinigameResultType::Fail) {
        assert(false);
    }*/
    return result;
}

MinigameResultType FishingMinigame::update() {

    mTickingTimer.startFrame();
    if (mStatus == MinigameResultType::InProgress) {
        if (!mTickingTimer.tryTick()) {
            return MinigameResultType::InProgress;
        }

        updatePlayerPosition();
        updateFishPosition();
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

    // Scaled transform
    const f32v2 boundarySize = getBoundarySize(mCurrentScreenResolution);
    const f32 screenScale = boundarySize.y / arenaSize.y;
    f32m4 camera(
        screenScale * (2.0f / mCurrentScreenResolution.x), 0, 0, 0,
        0, screenScale * (-2.0f / mCurrentScreenResolution.y), 0, 0,
        0, 0, 1.0f, 0,
        0, 0, 0, 1.0f
    );

    MaterialRenderer::bindMaterialForRender(*mUIShader);
    glUniformMatrix4fv(mUIShader->getUniform("unVP"), 1, false, &camera[0][0]);

    // Arena
    mUIParticleSystem->setParticleScale(mArenaParticleID, arenaSize);

    // Player
    mUIParticleSystem->setParticleScale(mPlayerParticleID, f32v2(mPlayerRadius * 2.0));
    mUIParticleSystem->setParticlePosition(mPlayerParticleID, f32v3(mPlayerPosition.x, mPlayerPosition.y, 0.0f));

    // Fish
    mUIParticleSystem->setParticleScale(mFishParticleID, f32v2(mFishRadius * 2.0f));
    mUIParticleSystem->setParticlePosition(mFishParticleID, f32v3(mFishPosition.x, mFishPosition.y, 0.0f));

    mUIParticleSystem->updateAndRender(mUIShader->mProgram, elapsedSec);
    mPlayerParticleSystem->updateAndRender(mUIShader->mProgram, elapsedSec);

    vg::DepthState::restorePrevious();
}

f32v2 getRandomDirectionVector() {
    // TOOD: Fishing internal stable random
    return glm::normalize(f32v2(Random::xorshf96f() * 2.0f - 1.0f, Random::xorshf96f() * 2.0f - 1.0f));
}

void FishingMinigame::updateFishPosition() {

    const FishingMinigameFishData& minigameData = mFishDef.mMinigameData;
    const TimePoint currentTime = mTickingTimer.getCurrTime();

    mFishVelocity += getRandomDirectionVector() * minigameData.mAcceleration;

    //  Drag
    mFishVelocity *= (1.0f - minigameData.mFishDrag);

    mFishPosition += mFishVelocity;

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
    mFishVelocity += -fishNormal * minigameData.mCenterMagnitism;

    // Gravity
    mFishVelocity.y -= minigameData.mGravity;

    // Player collision
    // Dont collide if we just took a fish life
    if (differenceBetweenTimePointsSeconds(currentTime, mLastFishLifeLostTime) >= FISH_LIFE_LOST_COOLDOWN) {
        const f32v2 offsetToPlayer = mPlayerPosition - mFishPosition;
        const f32 distanceFromPlayer = glm::length(offsetToPlayer);
        if (distanceFromPlayer <= mPlayerRadius + mFishRadius) {
            const f32v2 normalToPlayer = offsetToPlayer / distanceFromPlayer;
            mFishVelocity.x = lerp(mFishVelocity.x, mPlayerVelocity.x, minigameData.mPlayerStickyness.x);
            mFishVelocity.y = lerp(mFishVelocity.x, mPlayerVelocity.x, minigameData.mPlayerStickyness.y);
            mFishVelocity.y += minigameData.mPlayerStrength;
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
        if (getAngleOffset(fishNormal, f32v2(0.0f, 1.0f)) <= DEG_TO_RAD(minigameData.mSuccessAngle)) {
            fishLifeLost();
        }
        else if (getAngleOffset(fishNormal, f32v2(0.0f, -1.0f)) <= DEG_TO_RAD(minigameData.mFailAngle)) {
            playerLifeLost();
        }
    }
}

void FishingMinigame::updatePlayerPosition() {

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
    mPlayerVelocity += inputDir * minigameData.mPlayerAcceleration;
    //  Drag
    mPlayerVelocity *= (1.0f - minigameData.mPlayerDrag);

    // TMP SIMPLE VELOCITY
    //mPlayerVelocity = minigameData.mPlayerAcceleration * 2.0f * inputDir;

    mPlayerPosition += mPlayerVelocity;

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
    constexpr f32 FISH_LAUNCH_VEL = 3.0f;
    addDebugFloater("SUCCESS", getTextScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
    if (--mFishLivesLeft <= 0) {
        win();
        return;
    }
    mFishVelocity = f32v2(0.0f, -FISH_LAUNCH_VEL);
    mIsPlayerTouchingFish = false;
    mLastFishLifeLostTime = mTickingTimer.getCurrTime();
}

void FishingMinigame::playerLifeLost() {
    constexpr f32 FISH_LAUNCH_VEL = 3.0f;
    addDebugFloater("FAIL", getTextScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
    if (--mPlayerLivesLeft <= 0) {
        lose();
        return;
    }
    mFishVelocity = f32v2(0.0f, FISH_LAUNCH_VEL);
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
