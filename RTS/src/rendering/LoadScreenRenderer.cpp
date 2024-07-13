#include "stdafx.h"
#include "LoadScreenRenderer.h"

#include "rendering/RenderContext.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SpriteFont.h>
#include "ui/GameWindow.h"

const cString SIMPLE_VS_SRC = R"(
const vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));
out vec2 fUV;
void main() {
    gl_Position = vec4(vertices[gl_VertexID],0,1);
    fUV = 0.5 * gl_Position.xy + vec2(0.5);
}
)";
const cString SIMPLE_FS_SRC = R"(
uniform sampler2D unTexture;
uniform float unAspectRatio;

in vec2 fUV;

out vec4 fColor;

void main() {
    fColor = texture(unTexture, fUV);
}
)";

LoadScreenRenderer* LoadScreenRenderer::sInstance = nullptr;

static vg::GLProgram sProgram;

LoadScreenRenderer::LoadScreenRenderer() {
    sProgram = vg::ShaderManager::createProgram(SIMPLE_VS_SRC, SIMPLE_FS_SRC, nullptr);
    assert(sProgram.isLinked());
}

LoadScreenRenderer::~LoadScreenRenderer() {
    sProgram.dispose();
}

LoadScreenRenderer& LoadScreenRenderer::getInstance() {
    if (!sInstance) {
        sInstance = new LoadScreenRenderer();
    }
    return *sInstance;
}

void LoadScreenRenderer::render(OPT vui::GameWindow* windowToSync) {
    RenderContext& context = RenderContext::getInstance();

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    const f32v2& screenResolution = context.getScreenResolution();
    f32 aspectRatio = screenResolution.y / screenResolution.x;

    sProgram.use();
    //glUniform1f(*sProgram.tryGetUniform("unAspectRatio"), aspectRatio);
    if (mBackgroundTextures.size()) {
        const int textureIndex = mCurrentBackgroundTexture % mBackgroundTextures.size();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mBackgroundTextures[textureIndex]);
    }

    sGlobalFullTriangleVAO.draw();
    sProgram.unuse();

    // Spritefont text
    vg::SpriteFont& spriteFont = context.getSpriteFont();
    vg::SpriteBatch& spriteBatch = context.getSpriteBatch();

    spriteBatch.begin(30);
    spriteBatch.drawString(&spriteFont, mText.c_str(), screenResolution * f32v2(0.5f, 0.6f), f32v2(1.0f), COLOR_WHITE, vg::TextAlign::CENTER);
    spriteBatch.end();
    spriteBatch.render(screenResolution, &vg::sSamplerStates.LINEAR_WRAP);

    if (windowToSync) {
        windowToSync->sync(1);
    }
}

void LoadScreenRenderer::appendLoadingTexture(StrToken name, bool setActive) {
    TextureRepository& textureRepo = TextureRepository::get();
    mBackgroundTextures.push_back(textureRepo.getLoadedAsset(name).gpuTexture.getHandle());
    if (setActive) {
        mCurrentBackgroundTexture = mBackgroundTextures.size() - 1;
    }
}

void LoadScreenRenderer::setLoadingTexture(int index) {
    mCurrentBackgroundTexture = index;
}

void LoadScreenRenderer::setShowBar(bool showBar) {
    mShowBar = showBar;
}

void LoadScreenRenderer::setTotalWork(f32 totalWork) {
    mTotalWork = totalWork;
}

void LoadScreenRenderer::incWork(f32 workDone) {
    mWorkDone += workDone;
}

void LoadScreenRenderer::setWorkDone(f32 workDone) {
    mWorkDone = workDone;
}

void LoadScreenRenderer::setText(const nString& text) {
    mText = text;
}
