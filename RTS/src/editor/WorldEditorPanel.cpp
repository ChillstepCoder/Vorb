#include "stdafx.h"
#include "WorldEditorPanel.h"

#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "world/Chunk.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "options/DebugOptions.h"
#include "debugging/DebugRenderer.h"

#include "resources/ResourceOperations.h"
#include "resources/ResourceManager.h"
#include "ecs/EntityDefinitionRepository.h"
#include "editor/BrushRepository.h"
#include "resources/TileRepository.h"

#include "city/City.h"
#include "city/BuildingBlueprintGenerator.h"
#include "city/CityBuilder.h"
#include "city/BuildingDescriptionRepository.h"

#include "gamethread/GameThreadTasks.h"

#include "physics/PhysicsWorld.h"
#include "camera/Camera3D.h"

#include "math/Random.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/InputDispatcher.h>

#include "util/NativeFileBrowser.h"

constexpr f32 MIN_BRUSH_SIZE = 1.0f;
constexpr f32 MAX_BRUSH_SIZE = 50.0f;
constexpr f32 MIN_BRUSH_STRENGTH_TERRAIN = 0.01f;
constexpr f32 MAX_BRUSH_STRENGTH_TERRAIN = 2.0f;
constexpr f32 MIN_BRUSH_STRENGTH_GRASS = 0.01f;
constexpr f32 MAX_BRUSH_STRENGTH_GRASS = 1.0f;

WorldEditorPanel::WorldEditorPanel() {

    // Inputs
    vui::InputDispatcher::key.addKeyDownListener([this](const vui::KeyEvent& event) {

        if (!sDebugOptions.mShowEditor) return;

        if (event.keyCode == VKEY_RIGHT) {
            if (mCurrentBrushSettings) mCurrentBrushSettings->brushSize = glm::min(mCurrentBrushSettings->brushSize + 0.2f, MAX_BRUSH_SIZE);
        }
        else if (event.keyCode == VKEY_LEFT) {
            if (mCurrentBrushSettings) mCurrentBrushSettings->brushSize = glm::max(mCurrentBrushSettings->brushSize - 0.2f, MIN_BRUSH_SIZE);
        }
        else if (event.keyCode == VKEY_1) {
            setEditMode(WorldEditorEditMode::TERRAIN);
        }
        else if (event.keyCode == VKEY_2) {
            setEditMode(WorldEditorEditMode::GRASS);
        }
        else if (event.keyCode == VKEY_3) {
            setEditMode(WorldEditorEditMode::TILE);
        }
        else if (event.keyCode == VKEY_4) {
            setEditMode(WorldEditorEditMode::ENTITY);
        }
        else if (event.keyCode == VKEY_5) {
            setEditMode(WorldEditorEditMode::CITY);
        }
        else if (event.keyCode == VKEY_6) {
            setEditMode(WorldEditorEditMode::BUILDING);
        }
        static_assert((int)WorldEditorEditMode::COUNT == 6);
       
    });

    vui::InputDispatcher::mouse.addButtonUpListener([this](const vui::MouseButtonEvent& event) {
        if (!sDebugOptions.mShowEditor) return;

        if (event.button == vorb::ui::MouseButton::LEFT) {
            if (mEditMode == WorldEditorEditMode::CITY) {
                updateCityEdit();
            }
            else if (mEditMode == WorldEditorEditMode::BUILDING) {
                updateBuildingEdit();
            }
        }
    });
}

void WorldEditorPanel::update(const Camera3D& camera, const f32v3& pickRay) {
    PROFILE_FUNCTION();

    mUpdateTimer.startFrame();
    if (!mUpdateTimer.tryTick()) {
        return;
    }

    {
        PROFILE_SCOPE("Tile picking");
        mHitResult = mDeferredPhysicsPick.getLastPickResult();
        sWorld->getPhysicsWorld().pickDeferred(&mDeferredPhysicsPick, camera.getPosition(), camera.getPosition() + pickRay * 10000.0f, PICK_TYPE_ALL, PhysicsPickQueryFlags::QUERY_TILE_INFO);
    }

    if (mEditMode == WorldEditorEditMode::TERRAIN) {
        updateTerrainEdit();
    }
    else if (mEditMode == WorldEditorEditMode::GRASS) {
        updateGrassEdit();
    }
    else if (mEditMode == WorldEditorEditMode::TILE) {
        updateTileEdit();
    }
    else if (mEditMode == WorldEditorEditMode::ENTITY) {
        updateEntityEdit();
    }
    // City edit and edit building runs on mouse up
    static_assert((int)WorldEditorEditMode::COUNT == 6);
}

void WorldEditorPanel::renderBrushDecals (const Camera3D& camera) const {
    if (!mHitResult.didHit()) {
        return;
    }

    if (mCurrentBrushSettings) {
        f32v3 origin = mHitResult.mPosition - f32v3(mCurrentBrushSettings->brushSize, mCurrentBrushSettings->brushSize, 0.0f);
        f32v2 dims(mCurrentBrushSettings->brushSize * 2.0f);
        DebugRenderer::drawWireQuad(origin, dims, color4(0.0f, 0.0f, 1.0f, 0.9f));
    }
    else {
        f32v3 origin = f32v3((int)mHitResult.mPosition.x, (int)mHitResult.mPosition.y, mHitResult.mPosition.z);
        f32v2 dims(1.0f);
        DebugRenderer::drawWireQuad(origin, dims, color4(0.0f, 0.0f, 1.0f, 0.9f));
    }
}
// Use the manual it rocks
// https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
void WorldEditorPanel::renderUI(f32 ySize) const {
    const BrushRepository& brushRepo = Services::ResourceManager::ref().getBrushRepository();

    ImGui::BeginChild("World Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);

    renderMenuBar();

    // Add FPS for convenience
    char buffer[64];
    sprintf_s(buffer, sizeof(buffer), "FPS: %.0f", sFps);
    ImGui::Text(buffer);

    ImGui::Text("World Editor");
    ui32 ID = 10;

   // renderModeButtons();
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) { //ImGuiTabBarFlags_AutoSelectNewTabs

        if (ImGui::BeginTabItem("Terrain")) {
            setEditMode(WorldEditorEditMode::TERRAIN);
            renderTerrainEditUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Grass")) {
            setEditMode(WorldEditorEditMode::GRASS);
            renderGrassEditUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tile")) {
            setEditMode(WorldEditorEditMode::TILE);
            renderTileEditUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Entity")) {
            setEditMode(WorldEditorEditMode::ENTITY);
            renderEntityEditUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("City")) {
            setEditMode(WorldEditorEditMode::CITY);
            renderCityEditUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Building")) {
            setEditMode(WorldEditorEditMode::BUILDING);
            renderBuildingEditUI();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::NewLine();
    tryRenderBrushSelect(brushRepo);

    ImGui::EndChild();
    checkGlError("WorldEditor::renderUI()");
}

void WorldEditorPanel::renderMenuBar() const {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Model (FBX)")) {
                    const vio::Path filePath = NativeFileBrowser::openFile(FileBrowserTypes::FBX);
                    if (!filePath.isNull()) {
                        ResourceOperations::importFbxModel(filePath);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                vui::InputDispatcher::onQuit();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

//void WorldEditorPanel::renderModeButtons() const {
//    
//    // Helper for selected button styling
//#define PUSH_SELECTED_STYLE() \
//    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(1.0f, 0.6f, 0.6f)); \
//    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(1.0f, 0.7f, 0.7f)); \
//    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(1.0f, 0.8f, 0.8f));
//#define POP_SELECTED_STYLE() ImGui::PopStyleColor(3);
//#define SELECTED_BUTTON(b) PUSH_SELECTED_STYLE(); (b); POP_SELECTED_STYLE();
//
//    if (mEditMode == WorldEditorEditMode::TERRAIN) {
//        SELECTED_BUTTON(ImGui::Button("Terrain"));
//    }
//    else {
//        if (ImGui::Button("Terrain")) {
//            setEditMode(WorldEditorEditMode::TERRAIN);
//        }
//    }
//    ImGui::SameLine();
//
//    if (mEditMode == WorldEditorEditMode::GRASS) {
//        SELECTED_BUTTON(ImGui::Button("Grass"));
//    }
//    else if (ImGui::Button("Grass")) {
//        setEditMode(WorldEditorEditMode::GRASS);
//    }
//    ImGui::SameLine();
//
//    if (mEditMode == WorldEditorEditMode::TILE) {
//        SELECTED_BUTTON(ImGui::Button("Tile"));
//    }
//    else if (ImGui::Button("Tile")) {
//        setEditMode(WorldEditorEditMode::TILE);
//    }
//    ImGui::SameLine();
//
//    if (mEditMode == WorldEditorEditMode::ENTITY) {
//        SELECTED_BUTTON(ImGui::Button("Entity"));
//    }
//    else if (ImGui::Button("Entity")) {
//        setEditMode(WorldEditorEditMode::ENTITY);
//    }
//    ImGui::SameLine();
//
//    if (mEditMode == WorldEditorEditMode::CITY) {
//        SELECTED_BUTTON(ImGui::Button("City"));
//    }
//    else if (ImGui::Button("City")) {
//        setEditMode(WorldEditorEditMode::CITY);
//    }
//    ImGui::SameLine();
//
//    if (mEditMode == WorldEditorEditMode::BUILDING) {
//        SELECTED_BUTTON(ImGui::Button("Building"));
//    }
//    else if (ImGui::Button("Building")) {
//        setEditMode(WorldEditorEditMode::BUILDING);
//    }
//
//    static_assert((int)WorldEditorEditMode::COUNT == 6);
//}

void WorldEditorPanel::tryRenderBrushSelect(const BrushRepository& brushRepo) const {
    if (mCurrentBrushSettings) {
        if (ImGui::CollapsingHeader("Brushes", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
            const std::vector<Brush>& brushes = brushRepo.getBrushes();
            ImGui::Indent();
            ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
            for (size_t i = 0; i < brushes.size(); ++i) {
                const Brush& brush = brushes[i];
                ImGui::TableNextColumn();
                ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                if (ImGui::RadioButton(brush.name.c_str(), mCurrentBrushSettings->brushId == i)) {
                    mCurrentBrushSettings->brushId = (ui32)i;
                    mCurrentBrushSettings->activeBrush = &brush;
                }
                ImGui::TableNextColumn();
                ImGui::Image((ImTextureID)brush.texture, ImVec2(50.0f, 50.0f));
            }
            ImGui::EndTable();
            ImGui::Unindent();
        }
    }
}

void WorldEditorPanel::renderTerrainEditUI() const {
    ImGui::Text("Edit mode");
    if (ImGui::RadioButton("Lower", mTerrainEditState == TerrainEditState::LOWER_TERRAIN)) {
        mTerrainEditState = TerrainEditState::LOWER_TERRAIN;
    }
    if (ImGui::RadioButton("Raise", mTerrainEditState == TerrainEditState::RAISE_TERRAIN)) {
        mTerrainEditState = TerrainEditState::RAISE_TERRAIN;
    }
    if (ImGui::RadioButton("Flatten", mTerrainEditState == TerrainEditState::FLATTEN_TERRAIN)) {
        mTerrainEditState = TerrainEditState::FLATTEN_TERRAIN;
    }
    ImGui::SliderFloat("Brush Size", &mTerrainBrushSettings.brushSize, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Brush Strength", &mTerrainBrushSettings.brushStrength, MIN_BRUSH_STRENGTH_TERRAIN, MAX_BRUSH_STRENGTH_TERRAIN, "%.3f", ImGuiSliderFlags_Logarithmic);
}


void WorldEditorPanel::renderGrassEditUI() const {
    ImGui::Text("Edit mode");
    if (ImGui::RadioButton("Add", mGrassEditState == GrassEditState::ADD)) {
        mGrassEditState = GrassEditState::ADD;
    }
    if (ImGui::RadioButton("Remove", mGrassEditState == GrassEditState::REMOVE)) {
        mGrassEditState = GrassEditState::REMOVE;
    }
    ImGui::SliderFloat("Brush Size", &mGrassBrushSettings.brushSize, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Brush Strength", &mGrassBrushSettings.brushStrength, MIN_BRUSH_STRENGTH_GRASS, MAX_BRUSH_STRENGTH_GRASS, "%.3f", ImGuiSliderFlags_Logarithmic);
}

void WorldEditorPanel::renderTileEditUI() const {
    //ImGui::SliderInt("Floor", &mSelectedFloor, 0, TILE_FLOOR_COUNT - 1);
    ImGui::SliderFloat("Ground tile Z offset", &mGroundTileOffset, 0.0f, 10.0f, "%.2f");
    ImGui::Text("Tile select");
    const std::vector<TileData>& allData = TileRepository::getAllTileData();
    ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
    for (size_t i = 0; i < allData.size(); ++i) {
        const TileData& tileData = allData[i];
        ImGui::TableNextColumn();
        if (ImGui::RadioButton(tileData.name.c_str(), mSelectedTile == (ui32)i)) {
            mSelectedTile = (ui32)i;
        }
        ImGui::TableNextColumn();
        //ImGui::Image((ImTextureID)tileData.spriteData.texture, ImVec2(50.0f, 50.0f));
    }
    ImGui::EndTable();
}

void WorldEditorPanel::renderEntityEditUI() const {
    ImGui::Text("Select entity");
    const EntityDefinitionMap& entityDefs = Services::ResourceManager::ref().getEntityDefinitionRepository().getAllEntityDefinitions();
    const std::vector<TileData>& allData = TileRepository::getAllTileData();
    ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
    for (auto&& it : entityDefs) {
        ImGui::TableNextColumn();
        char buf[64];
        it.first.toString(buf, nullptr);
        if (ImGui::RadioButton(buf, mSelectedEntity == it.first)) {
            mSelectedEntity = it.first;
        }
        ImGui::TableNextColumn();
        //ImGui::Image((ImTextureID)tileData.spriteData.texture, ImVec2(50.0f, 50.0f));
    }
    ImGui::EndTable();
}

void WorldEditorPanel::renderCityEditUI() const {
    ImGui::Text("Edit mode");
    if (ImGui::RadioButton("None", mCityEditState == CityEditState::NONE)) {
        mCityEditState = CityEditState::NONE;
    }
    if (ImGui::RadioButton("Create City", mCityEditState == CityEditState::CREATE)) {
        mCityEditState = CityEditState::CREATE;
    }
    ImGui::Separator();
    ImGui::Text("Toggles");
    ImGui::Checkbox("Show City Debug", &sDebugOptions.mCities);
    ImGui::Checkbox("Show Roof Debug", &sDebugOptions.mRoofDebug);
}

void WorldEditorPanel::renderBuildingEditUI() const {
    ImGui::Text("Edit mode");
    if (ImGui::RadioButton("None", mBuildingEditState == BuildingEditState::NONE)) {
        mBuildingEditState = BuildingEditState::NONE;
    }
    if (ImGui::RadioButton("Create Building", mBuildingEditState == BuildingEditState::CREATE)) {
        mBuildingEditState = BuildingEditState::CREATE;
    }
    if (ImGui::RadioButton("Destroy Building", mBuildingEditState == BuildingEditState::DESTROY)) {
        mBuildingEditState = BuildingEditState::DESTROY;
    }
    ImGui::Separator();
    ImGui::Text("Toggles");
    ImGui::Checkbox("Show City Debug", &sDebugOptions.mCities);
    ImGui::Checkbox("Show Roof Debug", &sDebugOptions.mRoofDebug);

    ImGui::Separator();
    ImGui::Text("Building select");
    const std::vector<BuildingDef> buildings = Services::ResourceManager::ref().getBuildingDescriptionRepository().getBuildingDefs();
    ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
    for (size_t i = 0; i < buildings.size(); ++i) {
        const BuildingDef& buildingDef = buildings[i];
        ImGui::TableNextColumn();
        char buf[64];
        buildingDef.nameToken.toString(buf, nullptr);
        if (ImGui::RadioButton(buf, mSelectedBuilding == (ui32)i)) {
            mSelectedBuilding = (ui32)i;
        }
        ImGui::TableNextColumn();
        ImGui::Text("Size Range <%u,%u>", buildingDef.widthRange.x, buildingDef.widthRange.y);
        //ImGui::Image((ImTextureID)tileData.spriteData.texture, ImVec2(50.0f, 50.0f));
    }
    ImGui::EndTable();
    const BuildingDef& selectedDef = buildings[mSelectedBuilding];
    mPlotDims.x = glm::clamp(mPlotDims.x, (i32)selectedDef.widthRange.x, (i32)selectedDef.widthRange.y);
    mPlotDims.y = glm::clamp(mPlotDims.y, (i32)selectedDef.widthRange.x, (i32)selectedDef.widthRange.y);
    ImGui::DragInt2("Plot Dims (x,y)", &mPlotDims.x, 0.5f, selectedDef.widthRange.x, selectedDef.widthRange.y);
}

void WorldEditorPanel::updateTerrainEdit() {

    if (!mCurrentBrushSettings || !mCurrentBrushSettings->activeBrush) {
        return;
    }

    //PreciseTimer timer;
    // Pick terrain
    //std::cout << "TERRAIN PICK MS " << timer.stop() << std::endl;
    if (mHitResult.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {

            struct TerrainEditTask {
                PhysHitResult hitResult;
                BrushSettings brushSettings;
                TerrainEditState editState;
            };
            TerrainEditTask* task = new TerrainEditTask;
            task->hitResult = mHitResult;
            task->brushSettings = *mCurrentBrushSettings;
            task->editState = mTerrainEditState;

            GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTask) {
                const TerrainEditTask* task = static_cast<TerrainEditTask*>(vTask);
                const PhysHitResult& hitResult = task->hitResult;
                const BrushSettings& brushSettings = task->brushSettings;

                PreciseTimer timer;
                // Edit the terrain with iteration
                const f32v2 hitPosition2D(hitResult.mPosition.x, hitResult.mPosition.y);
                const f32v2 worldPosBrushStart = hitPosition2D - f32v2(brushSettings.brushSize);
                const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(brushSettings.brushSize);
                const f32 brushSizeSq = SQ(brushSettings.brushSize);
                f32v2 worldPos;
                for (worldPos.y = worldPosBrushStart.y; worldPos.y <= worldPosBrushEnd.y + HEIGHTMAP_QUAD_SIZE; worldPos.y += HEIGHTMAP_QUAD_SIZE) {
                    for (worldPos.x = worldPosBrushStart.x; worldPos.x <= worldPosBrushEnd.x + HEIGHTMAP_QUAD_SIZE; worldPos.x += HEIGHTMAP_QUAD_SIZE) {
                        HeightmapPatchID id(worldPos);
                        const f32v2 terrainWorldPos = id.getWorldPos();
                        const f32v2 offset = worldPos - terrainWorldPos;
                        const ui32v2 vertexPos = ui32v2(offset / (f32)HEIGHTMAP_QUAD_SIZE);
                        const f32v2 vertexPosWorld = f32v2(vertexPos) * (f32)HEIGHTMAP_QUAD_SIZE + terrainWorldPos;
                        const f32v2 offsetToVertex = hitPosition2D - vertexPosWorld;
                        if (glm::length2(offsetToVertex) < brushSizeSq) {
                            editVertex(id, vertexPos, offsetToVertex, brushSettings, task->editState);
                        }
                    }
                }

                // Notify all terrain stuff to update
                sWorld->dirtyTerrainFromBrush(f32v2(hitResult.mPosition.x, hitResult.mPosition.y), brushSettings.brushSize + HEIGHTMAP_QUAD_SIZE);

                delete task;
            }, task);
        }
    }
}

void WorldEditorPanel::updateGrassEdit() {

    if (!mCurrentBrushSettings || !mCurrentBrushSettings->activeBrush) {
        return;
    }

    if (mHitResult.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {

            struct GrassEditTask {
                PhysHitResult hitResult;
                BrushSettings brushSettings;
                GrassEditState editState;
            };
            GrassEditTask* task = new GrassEditTask;
            task->hitResult = mHitResult;
            task->brushSettings = *mCurrentBrushSettings;
            task->editState = mGrassEditState;

            GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTask) {
                const GrassEditTask* task = static_cast<GrassEditTask*>(vTask);
                const PhysHitResult& hitResult = task->hitResult;
                const BrushSettings& brushSettings = task->brushSettings;
                PreciseTimer timer;
                // Edit the terrain with iteration
                const f32v2 hitPosition2D(hitResult.mPosition.x, hitResult.mPosition.y);
                const f32v2 worldPosBrushStart = hitPosition2D - f32v2(brushSettings.brushSize);
                const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(brushSettings.brushSize);
                const f32 brushSizeSq = SQ(brushSettings.brushSize);
                f32v2 worldPos;
                {
                    PROFILE_SCOPE("Edit Grass");
                    for (worldPos.y = worldPosBrushStart.y; worldPos.y <= worldPosBrushEnd.y; worldPos.y += 1.0f) {
                        for (worldPos.x = worldPosBrushStart.x; worldPos.x <= worldPosBrushEnd.x; worldPos.x += 1.0f) {
                            ChunkID id(worldPos);
                            const TileContainer& tileContainer = *sWorld->getChunk(id).getTileContainer();
                            TileIndex tileIndex = tileContainer.getTileIndexFromXYZOffset((ui32)worldPos.x % CHUNK_WIDTH, (ui32)worldPos.y % CHUNK_WIDTH, 0);
                            const f32v2 tilePosWorld = worldPos + f32v2(0.5f, 0.5f);
                            const f32v2 offsetToTile = hitPosition2D - tilePosWorld;
                            if (glm::length2(offsetToTile) < brushSizeSq) {
                                editGrass(id, tileIndex, offsetToTile, brushSettings, task->editState);
                            }
                        }
                    }
                }

         
                // Notify all terrain stuff to update
                sWorld->dirtyGrassFromBrush(f32v2(hitResult.mPosition.x, hitResult.mPosition.y), brushSettings.brushSize + 1);

            delete task;
            }, task);
        }
    }
}

void WorldEditorPanel::updateTileEdit() {
    static ChunkID prevChunkID;
    static TileIndex prevTileIndex;
    if (mHitResult.didHit() && vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
        const ChunkID chunkID(f32v2(mHitResult.mPosition.x, mHitResult.mPosition.y));
        const TileIndex tileIndex = (TileIndex)((ui32)mHitResult.mPosition.x % CHUNK_WIDTH + ((ui32)mHitResult.mPosition.y % CHUNK_WIDTH) * CHUNK_WIDTH);

        // Make sure while mouse is held we aren't spamming tiles in the same spot
        if (chunkID != prevChunkID || tileIndex != prevTileIndex) {
            prevChunkID = chunkID;
            prevTileIndex = tileIndex;

            std::tuple<LiteChunkID, TileIndex, TileID>* taskData = new std::tuple<LiteChunkID, TileIndex, TileID>(chunkID.id, tileIndex, mSelectedTile);
            GameThreadTasks::getInstance().addGenericTask([](GameThread& gameThread, void* v) {
                std::tuple<LiteChunkID, TileIndex, TileID>* taskData = (std::tuple<LiteChunkID, TileIndex, TileID>*)v;
                LiteChunkID chunkId = std::get<0>(*taskData);
                Chunk& chunk = sWorld->getChunk(chunkId);
                if (chunk.isDataReady()) {
                    TileIndex tileIndex = std::get<1>(*taskData);
                    const TileData& data = TileRepository::getTileData(std::get<2>(*taskData));
                    TileContainer& tileContainer = *chunk.getTileContainer();
                    tileContainer.setTileLayer(tileIndex, data);
                }
                delete taskData;
            }, taskData);
        }
    }
    else {
        prevChunkID = 0;
    }
}

void WorldEditorPanel::updateEntityEdit() {
    if (mHitResult.didHit() && vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT) && mSelectedEntity) {
        GameThreadTasks::getInstance().addEntityCreateTask(mHitResult.mPosition, mSelectedEntity, true);
    }
}

void WorldEditorPanel::updateCityEdit() {
    // Happens on mouse up
    if (mHitResult.didHit() && mCityEditState == CityEditState::CREATE) {

        struct CityCreateTask {
            f32v2 worldPos;
        };
        CityCreateTask* task = new CityCreateTask;
        task->worldPos = f32v2(mHitResult.mPosition.x, mHitResult.mPosition.y);
        GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTask) {
            CityCreateTask* task = static_cast<CityCreateTask*>(vTask);
            sWorld->getCityGraph().createCityAt(ui32v2(floor(task->worldPos.x), floor(task->worldPos.y)));
            delete task;
        }, task);
    }
}

void WorldEditorPanel::updateBuildingEdit() {
    // Happens on mouse up
    if (mHitResult.didHit() && mBuildingEditState == BuildingEditState::CREATE) {
        f32v2 worldPos(mHitResult.mPosition.x, mHitResult.mPosition.y);
        ui32v2 createPos(floor(worldPos.x), floor(worldPos.y));

        struct BuildingEditCreateTask {
            i32AABB2 aabb;
            ui32 selectedBuildingId;
        };
        BuildingEditCreateTask* task = new BuildingEditCreateTask;
        task->aabb.pos = createPos;
        task->aabb.dims = mPlotDims;
        task->selectedBuildingId = mSelectedBuilding;

        GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTask) {
            BuildingEditCreateTask* task = static_cast<BuildingEditCreateTask*>(vTask);
            const f32 meanHeight = round(sHeightmapGrid->computeMeanHeightAtAABB(task->aabb));
            BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingDescriptionRepository();
            std::unique_ptr<BuildingBlueprint> bp = BuildingBlueprintGenerator::generateBlueprintSync(buildingRepo, buildingRepo.getBuildingDef(task->selectedBuildingId), 1.0f /*?*/, Cartesian::WEST, task->aabb.dims, task->aabb.pos, INVALID_ENTITY, BuildingBlueprintFlags(0), meanHeight);
            CityBuilder::debugBuildInstant(*bp);
            delete task;
        }, task);

    }
}

void WorldEditorPanel::editVertex(HeightmapPatchID id, const ui32v2& vertPos, const f32v2& offsetToVertex, const BrushSettings& brush, TerrainEditState editState) {
    
    // Read brush data
    f32 strength = getBrushStrengthAtPoint(brush, offsetToVertex);

    if (strength > 0.001f) {
        switch (editState) {
            case TerrainEditState::RAISE_TERRAIN:
                break;
            case TerrainEditState::LOWER_TERRAIN:
                strength = -strength;
                break;
            case TerrainEditState::FLATTEN_TERRAIN:
                break;
            default:
                assert(false);
                break;
        }
        static_assert((int)TerrainEditState::COUNT == 3, "Update for new edit type");
        const f32 adjust = strength * brush.brushStrength;
        sHeightmapGrid->adjustHeightAt(id, vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + vertPos.x, adjust);

        // Debug render
        f32v2 chunkPos = id.getWorldPos();
        f32v2 dims(0.5f);
        f32v3 worldPos(chunkPos.x + vertPos.x * HEIGHTMAP_QUAD_SIZE - dims.x * 0.5f, chunkPos.y + vertPos.y * HEIGHTMAP_QUAD_SIZE - dims.y * 0.5f, sHeightmapGrid->getHeightAtVert(id, vertPos) + adjust);
        DebugRenderer::drawWireQuadThreadSafe(worldPos, dims, color4(1.0f, 0.0f, 1.0f, abs(strength)), 3);
    }
}

void WorldEditorPanel::editGrass(ChunkID id, TileIndex tileIndex, const f32v2& offsetToTile, const BrushSettings& brush, GrassEditState editState) {

    f32 strength = getBrushStrengthAtPoint(brush, offsetToTile) * brush.brushStrength;
    const f32 random = Random::getCachedRandomfSpecific(id.id * CHUNK_SIZE + tileIndex);
    if (random < strength) {
        if (editState == GrassEditState::ADD) {
            sWorld->getChunk(id).setGrassAt(tileIndex, 1);
        }
        else {
            sWorld->getChunk(id).setGrassAt(tileIndex, 0);
        }
    }
}

f32 WorldEditorPanel::getBrushStrengthAtPoint(const BrushSettings& brush, const f32v2& brushOffsetToPoint) {
    f32v2 offsetToCornerNormalized = (brushOffsetToPoint + f32v2(brush.brushSize)) / f32v2(brush.brushSize * 2.0f);
    if (offsetToCornerNormalized.x < 0.0f || offsetToCornerNormalized.y < 0.0f) {
        return 0.0f;
    }
    ui32v2 pixelPos = offsetToCornerNormalized * f32v2(brush.activeBrush->dims.x, brush.activeBrush->dims.y);
    if (pixelPos.x >= brush.activeBrush->dims.x || pixelPos.y >= brush.activeBrush->dims.y) {
        return 0.0f;
    }
    ui8 brushIntensity = brush.activeBrush->data[pixelPos.y * brush.activeBrush->dims.x + pixelPos.x];
    return (f32)brushIntensity / 255.0f;
}

void WorldEditorPanel::setEditMode(WorldEditorEditMode mode) const {
    mEditMode = mode;
    switch (mEditMode) {
        case WorldEditorEditMode::TERRAIN:
            mCurrentBrushSettings = &mTerrainBrushSettings;
            break;
        case WorldEditorEditMode::GRASS:
            mCurrentBrushSettings = &mGrassBrushSettings;
            break;
        case WorldEditorEditMode::TILE:
        case WorldEditorEditMode::ENTITY:
        case WorldEditorEditMode::CITY:
        case WorldEditorEditMode::BUILDING:
            mCurrentBrushSettings = nullptr;
            break;
        default:
            assert(false);
    }
    static_assert((int)WorldEditorEditMode::COUNT == 6, "Update");
}

