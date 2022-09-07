#include "stdafx.h"
#include "Test3DScreen.h"

#include "App.h"

#include "resources/ResourceManager.h"

#include "rendering/TileVertex.h"

#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"

#include <Vorb/ui/InputDispatcher.h>

#include "world/World.h"

#if IS_ENABLED(FEATURE_TEST_3D)

const f32v3 VOXEL_MODEL[24] = {
    f32v3(0, 1, 0),
    f32v3(0, 1, 1),
    f32v3(0, 0, 0),
    f32v3(0, 0, 1),

    f32v3(1, 1, 1),
    f32v3(1, 1, 0),
    f32v3(1, 0, 1),
    f32v3(1, 0, 0),

    f32v3(0, 0, 1),
    f32v3(1, 0, 1),
    f32v3(0, 0, 0),
    f32v3(1, 0, 0),

    f32v3(0, 1, 0),
    f32v3(1, 1, 0),
    f32v3(0, 1, 1),
    f32v3(1, 1, 1),

    f32v3(1, 1, 0),
    f32v3(0, 1, 0),
    f32v3(1, 0, 0),
    f32v3(0, 0, 0),

    f32v3(0, 1, 1),
    f32v3(1, 1, 1),
    f32v3(0, 0, 1),
    f32v3(1, 0, 1)
};

const ui32 VOXEL_INDICES[6] = {
    0, 2, 1,
    1, 2, 3
};

const i8v3 VOXEL_SIDES[6] = {
    i8v3(-1, 0, 0),
    i8v3(1, 0, 0),
    i8v3(0, -1, 0),
    i8v3(0, 1, 0),
    i8v3(0, 0, -1),
    i8v3(0, 0, 1),
};

const color4 VOXEL_FACE_COLORS[6] = {
    color4(1.0f, 0.0f, 0.0f, 1.0f),
    color4(1.0f, 1.0f, 0.0f, 1.0f),
    color4(0.0f, 0.0f, 1.0f, 1.0f),
    color4(1.0f, 0.0f, 1.0f, 1.0f),
    color4(1.0f, 1.0f, 0.0f, 1.0f),
    color4(1.0f, 1.0f, 1.0f, 1.0f)
};

void genCubeMesh(std::vector<TileVertex>& vertices) {
    // Single cube test
    for (int face = 0; face < 6; face++) { // For each face of the voxel
        int indexStart = (int)vertices.size();

        // Add the 4 vertices for this face
        vertices.resize(indexStart + 4);
        for (int l = 0; l < 4; l++) {
            vertices[indexStart + l].pos = VOXEL_MODEL[face * 4 + l];
            vertices[indexStart + l].color = VOXEL_FACE_COLORS[face];
            vertices[indexStart + l].normal = i8v3(VOXEL_SIDES[face]);
            vertices[indexStart + l].atlasPage = 0;
            vertices[indexStart + l].uvs = f32v2(0.0f);
            vertices[indexStart + l].height = 0;
            vertices[indexStart + l].shadowState = 0;
        }
    }
}

Test3DScreen::Test3DScreen(const App* app)
    : IAppScreen<App>(app),
    mResourceManager(&Services::ResourceManager::ref()),
    mRenderContext(RenderContext::initInstance(*mResourceManager, *mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()))),
    mWorld(std::make_unique<World>(*mResourceManager))
{

}

Test3DScreen::~Test3DScreen()
{

}

i32 Test3DScreen::getNextScreen() const {
    return 0;
}

i32 Test3DScreen::getPreviousScreen() const {
    return 0;
}

void Test3DScreen::build() {
    const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
    mCamera.init(screenSize.x / screenSize.y);

    mResourceManager->gatherFiles("data");
    mResourceManager->loadFiles();

    mRenderContext.initPostLoad();

    // Test mesh
    mQuadMesh = std::make_unique<QuadMesh>();
    std::vector<TileVertex> tileData;
    genCubeMesh(tileData);
    mQuadMesh->setData(tileData.data(), tileData.size(), 0, MeshDrawMove::STATIC);

    // Camera
    mCamera.setPosition(f64v3(-10.0f, -2.0f, 0.0f));
}

void Test3DScreen::destroy(const vui::GameTime& gameTime) {
}

void Test3DScreen::onEntry(const vui::GameTime& gameTime) { 
}

void Test3DScreen::onExit(const vui::GameTime& gameTime) {
}

void Test3DScreen::update(const vui::GameTime& gameTime)
{

    f32v3 position = mCamera.getPosition();
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT)) {
        position.z -= 0.1f;
    }else if (vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT)) {
        position.z += 0.1f;
    }

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_UP)) {
        position.y += 0.1f;
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_DOWN)) {
        position.y -= 0.1f;
    }

    f32 fov = mCamera.getFieldOfView();
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_A)) {
        fov -= 0.4f;
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_D)) {
        fov += 0.4f;
    }
    std::cout << fov << std::endl;
    fov = glm::clamp(fov, 1.0f, 179.0f);
    mCamera.setFieldOfView(fov);
    mCamera.setPosition(f64v3(position));
    mCamera.lookAt(f32v3(0.5f));


    mCamera.update();
}

void Test3DScreen::draw(const vui::GameTime& gameTime) {
    const f32m4& vp = mCamera.getVPMatrix();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    mRenderContext.beginFrame(&mCamera, f32v3(0.0f), f32v2(0.0f));

    const Material* material = mResourceManager->getMaterialManager().getMaterial("standard_tile");
    mRenderContext.getMaterialRenderer().renderMesh(*mQuadMesh, *material);

    checkGlError("RenderContext::FrameEnd");
}

#endif

