#include "stdafx.h"
#include "FishingMinigame.h"

#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderManager.h"

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/ui/InputDispatcher.h>

#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/FullQuadVBO.h>
#include "rendering/RenderContext.h"
#include "rendering/MaterialRenderer.h"

#include "math/Random.h"

constexpr f32 BOUNDARY_RADIUS = 100.0f;
constexpr f32 FISH_RADIUS = BOUNDARY_RADIUS * 0.3f;
constexpr f32 BOUNDARY_RADIUS_SQ = SQ(BOUNDARY_RADIUS);
constexpr f32 FISH_LIFE_LOST_COOLDOWN = 0.35f;

constexpr f32 END_TRANSITION_TIME = 0.75f;

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
    mPlayerPosition = f32v2(0.0f, BOUNDARY_RADIUS - mPlayerRadius - 1);
    mSpriteBatch.init();
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    mTextureCircle = resourceManager.getMaterialRepository().getMaterialDesc("fishing_circle").albedoTexture;
    mArenaShader = resourceManager.getMaterialShaderManager().getMaterialShader("fishing_arena");

}

FishingMinigame::~FishingMinigame()
{
}

FishingMinigameResult FishingMinigame::updateAndRender(const f32v2 screenResolution) {
    mCurrentScreenResolution = screenResolution;

    mFishRadius = getFishRadius(mFishDef);

    FishingMinigameResult result;
    result.result = update();
    render();
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

f32v2 getScreenPosition(f32v2 gamePosition, const f32v2 screenResolution) {
    const f32v2 boundarySize = getBoundarySize(screenResolution);
    const f32 screenScale = boundarySize.y / (BOUNDARY_RADIUS * 2.0);
    return gamePosition * screenScale + screenResolution * 0.5f; // Offset to center since game origin in center
}

void FishingMinigame::render() {
    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    const FishingMinigameFishData& minigameData = mFishDef.mMinigameData;
    const f32v2 boundarySize = getBoundarySize(mCurrentScreenResolution);
    const f32v2 centerPos = mCurrentScreenResolution * 0.5f;

    // Scaled
    const f32 screenScale = boundarySize.y / (BOUNDARY_RADIUS * 2.0);
    const f32v2 fishPos = centerPos + mFishPosition * screenScale;
    const f32v2 playerPos = centerPos + mPlayerPosition * screenScale;
    const f32v2 fishSize = f32v2(mFishRadius * screenScale * 2.0);
    const f32v2 playerSize = f32v2(mPlayerRadius * screenScale * 2.0);

    // Arena
    constexpr int TEXTURE_UNIT = 0;
    MaterialRenderer::bindMaterialForRender(*mArenaShader);
    glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT);
    glUniform1i(mArenaShader->getUniform("unTexture"), TEXTURE_UNIT);
    glUniform1f(mArenaShader->getUniform("unRadius"), boundarySize.y * 0.5f);
    glUniform1f(mArenaShader->getUniform("unSuccessAngle"), DEG_TO_RAD(minigameData.mSuccessAngle));
    glUniform1f(mArenaShader->getUniform("unFailAngle"), DEG_TO_RAD(minigameData.mFailAngle));
    // TODO: Util
    f32m4 camera(
        2.0f / mCurrentScreenResolution.x, 0, 0, 0,
        0, -2.0f / mCurrentScreenResolution.y, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1
    );  
    glUniformMatrix4fv(mArenaShader->getUniform("unVP"), 1, false, &camera[0][0]);
    glBindTextureUnit(TEXTURE_UNIT, mTextureCircle);
    sGlobalFullQuadVBO.draw();

    mSpriteBatch.begin(4);

   // mSpriteBatch.draw(mTextureCircle, centerPos - boundarySize * 0.5f, boundarySize, color::Gray);
    if (fishSize.x > playerSize.x) {
        mSpriteBatch.draw(mTextureCircle, fishPos - fishSize * 0.5f, fishSize, mIsPlayerTouchingFish ? color::Yellow : color::Red);
        mSpriteBatch.draw(mTextureCircle, playerPos - playerSize * 0.5f, playerSize, color::Green);
    }
    else {
        // Draw fish on bottom if hes smaller
        mSpriteBatch.draw(mTextureCircle, playerPos - playerSize * 0.5f, playerSize, color::Green);
        mSpriteBatch.draw(mTextureCircle, fishPos - fishSize * 0.5f, fishSize, mIsPlayerTouchingFish ? color::Yellow : color::Red);
    }

    renderDebugFloaters();

    mSpriteBatch.end(vg::SpriteSortMode::NONE);
    mSpriteBatch.render(mCurrentScreenResolution);

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
            addDebugFloater("JERK", getScreenPosition(mFishPosition, mCurrentScreenResolution), color::White);
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
    mPlayerVelocity = minigameData.mPlayerAcceleration * 2.0f * inputDir;

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
    addDebugFloater("SUCCESS", getScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
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
    addDebugFloater("FAIL", getScreenPosition(mFishPosition, mCurrentScreenResolution), color::Green);
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
