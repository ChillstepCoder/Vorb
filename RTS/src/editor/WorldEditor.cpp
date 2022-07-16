#include "stdafx.h"
#include "WorldEditor.h"

#include "World.h"
#include "world/WorldGrid.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "world/Chunk.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "options/DebugOptions.h"
#include "DebugRenderer.h"

#include "ResourceManager.h"
#include "ecs/EntityDefinitionRepository.h"
#include "editor/BrushRepository.h"
#include "resources/TileRepository.h"

#include "city/City.h"
#include "city/BuildingBlueprintGenerator.h"
#include "city/CityBuilder.h"
#include "city/BuildingDescriptionRepository.h"

#include "physics/PhysicsWorld.h"
#include "camera/Camera3D.h"

#include "Random.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/InputDispatcher.h>

constexpr f32 MIN_BRUSH_SIZE = 1.0f;
constexpr f32 MAX_BRUSH_SIZE = 50.0f;
constexpr f32 MIN_BRUSH_STRENGTH_TERRAIN = 0.01f;
constexpr f32 MAX_BRUSH_STRENGTH_TERRAIN = 2.0f;
constexpr f32 MIN_BRUSH_STRENGTH_GRASS = 0.01f;
constexpr f32 MAX_BRUSH_STRENGTH_GRASS = 1.0f;

WorldEditor::WorldEditor(World& world, const f32v2& screenDims) : mWorld(world), mScreenDims(screenDims) {

    // Inputs
    vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {

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

    vui::InputDispatcher::mouse.onButtonUp.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
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

void WorldEditor::update(const Camera3D& camera) {
    const f32v3& pickRay = sDebugOptions.mMousePickRay;

    PreciseTimer timer;
    mHitResult = mWorld.getPhysicsWorld().pick(camera.getPosition(), camera.getPosition() + sDebugOptions.mMousePickRay * 10000.0f, PICK_TYPE_ALL);

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

void WorldEditor::renderBrushDecals (const Camera3D& camera) const {
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
void WorldEditor::renderUI() const {
    const BrushRepository& brushRepo = Services::ResourceManager::ref().getBrushRepository();

    constexpr float WINDOW_WIDTH = 400.0f;
    const float WINDOW_HEIGHT = mScreenDims.y;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    const ImVec2 buttonSize(WINDOW_WIDTH, 25);

    ImGui::Begin("World Editor", &sDebugOptions.mShowEditor, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    renderModeButtons();

    switch (mEditMode) {
        case WorldEditorEditMode::TERRAIN:
            renderTerrainEditUI();
            break;
        case WorldEditorEditMode::GRASS:
            renderGrassEditUI();
            break;
        case WorldEditorEditMode::TILE:
            renderTileEditUI();
            break;
        case WorldEditorEditMode::ENTITY:
            renderEntityEditUI();
            break;
        case WorldEditorEditMode::CITY:
            renderCityEditUI();
            break;
        case WorldEditorEditMode::BUILDING:
            renderBuildingEditUI();
            break;
        default:
            assert(false);
    }
    static_assert((int)WorldEditorEditMode::COUNT == 6);

    ImGui::NewLine();
    tryRenderBrushSelect(brushRepo);

    ImGui::End();
    checkGlError("WorldEditor::renderUI()");
}


void WorldEditor::renderModeButtons() const {
    
    // Helper for selected button styling
#define PUSH_SELECTED_STYLE() \
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(1.0f, 0.6f, 0.6f)); \
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(1.0f, 0.7f, 0.7f)); \
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(1.0f, 0.8f, 0.8f));
#define POP_SELECTED_STYLE() ImGui::PopStyleColor(3);
#define SELECTED_BUTTON(b) PUSH_SELECTED_STYLE(); (b); POP_SELECTED_STYLE();

    if (mEditMode == WorldEditorEditMode::TERRAIN) {
        SELECTED_BUTTON(ImGui::Button("Terrain"));
    }
    else {
        if (ImGui::Button("Terrain")) {
            setEditMode(WorldEditorEditMode::TERRAIN);
        }
    }
    ImGui::SameLine();

    if (mEditMode == WorldEditorEditMode::GRASS) {
        SELECTED_BUTTON(ImGui::Button("Grass"));
    }
    else if (ImGui::Button("Grass")) {
        setEditMode(WorldEditorEditMode::GRASS);
    }
    ImGui::SameLine();

    if (mEditMode == WorldEditorEditMode::TILE) {
        SELECTED_BUTTON(ImGui::Button("Tile"));
    }
    else if (ImGui::Button("Tile")) {
        setEditMode(WorldEditorEditMode::TILE);
    }
    ImGui::SameLine();

    if (mEditMode == WorldEditorEditMode::ENTITY) {
        SELECTED_BUTTON(ImGui::Button("Entity"));
    }
    else if (ImGui::Button("Entity")) {
        setEditMode(WorldEditorEditMode::ENTITY);
    }
    ImGui::SameLine();

    if (mEditMode == WorldEditorEditMode::CITY) {
        SELECTED_BUTTON(ImGui::Button("City"));
    }
    else if (ImGui::Button("City")) {
        setEditMode(WorldEditorEditMode::CITY);
    }
    ImGui::SameLine();

    if (mEditMode == WorldEditorEditMode::BUILDING) {
        SELECTED_BUTTON(ImGui::Button("Building"));
    }
    else if (ImGui::Button("Building")) {
        setEditMode(WorldEditorEditMode::BUILDING);
    }

    static_assert((int)WorldEditorEditMode::COUNT == 6);
}

void WorldEditor::tryRenderBrushSelect(const BrushRepository& brushRepo) const {
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

void WorldEditor::renderTerrainEditUI() const {
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


void WorldEditor::renderGrassEditUI() const {
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

void WorldEditor::renderTileEditUI() const {
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

void WorldEditor::renderEntityEditUI() const {
    ImGui::Text("Select entity");
    const EntityDefinitionMap& entityDefs = Services::ResourceManager::ref().getEntityDefinitionRepository().getAllEntityDefinitions();
    const std::vector<TileData>& allData = TileRepository::getAllTileData();
    ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
    for (auto&& it : entityDefs) {
        ImGui::TableNextColumn();
        if (ImGui::RadioButton(it.first.c_str(), mSelectedEntity == it.first)) {
            mSelectedEntity = it.first;
        }
        ImGui::TableNextColumn();
        //ImGui::Image((ImTextureID)tileData.spriteData.texture, ImVec2(50.0f, 50.0f));
    }
    ImGui::EndTable();
}

void WorldEditor::renderCityEditUI() const {
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

void WorldEditor::renderBuildingEditUI() const {
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
    const std::vector<BuildingDef> buildings = Services::ResourceManager::ref().getBuildingRepository().getBuildingDefs();
    ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
    for (size_t i = 0; i < buildings.size(); ++i) {
        const BuildingDef& buildingDef = buildings[i];
        ImGui::TableNextColumn();
        if (ImGui::RadioButton(buildingDef.name.c_str(), mSelectedBuilding == (ui32)i)) {
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

void WorldEditor::updateTerrainEdit() {

    if (!mCurrentBrushSettings || !mCurrentBrushSettings->activeBrush) {
        return;
    }

    //PreciseTimer timer;
    // Pick terrain
    //std::cout << "TERRAIN PICK MS " << timer.stop() << std::endl;
    if (mHitResult.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
            PreciseTimer timer;
            // Edit the terrain with iteration
            const f32v2 hitPosition2D(mHitResult.mPosition.x, mHitResult.mPosition.y);
            const f32v2 worldPosBrushStart = hitPosition2D - f32v2(mCurrentBrushSettings->brushSize);
            const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(mCurrentBrushSettings->brushSize);
            const f32 brushSizeSq = SQ(mCurrentBrushSettings->brushSize);
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
                        editVertex(id, vertexPos, offsetToVertex);
                    }
                }
            }

            // Notify all terrain stuff to update
            mWorld.dirtyTerrainFromBrush(f32v2(mHitResult.mPosition.x, mHitResult.mPosition.y), mCurrentBrushSettings->brushSize + HEIGHTMAP_QUAD_SIZE);
            std::cout << "TERRAIN FLOOD MS " << timer.stop() << std::endl;
        }
    }
}

void WorldEditor::updateGrassEdit() {

    if (!mCurrentBrushSettings || !mCurrentBrushSettings->activeBrush) {
        return;
    }

    if (mHitResult.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
            PreciseTimer timer;
            // Edit the terrain with iteration
            const f32v2 hitPosition2D(mHitResult.mPosition.x, mHitResult.mPosition.y);
            const f32v2 worldPosBrushStart = hitPosition2D - f32v2(mCurrentBrushSettings->brushSize);
            const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(mCurrentBrushSettings->brushSize);
            const f32 brushSizeSq = SQ(mCurrentBrushSettings->brushSize);
            f32v2 worldPos;
            for (worldPos.y = worldPosBrushStart.y; worldPos.y <= worldPosBrushEnd.y; worldPos.y += 1.0f) {
                for (worldPos.x = worldPosBrushStart.x; worldPos.x <= worldPosBrushEnd.x; worldPos.x += 1.0f) {
                    ChunkID id(worldPos);
                    const TileContainer& tileContainer = *mWorld.getChunk(id).getTileContainer();
                    TileIndex tileIndex = tileContainer.getTileIndexFromXYZOffset((ui32)worldPos.x % CHUNK_WIDTH, (ui32)worldPos.y % CHUNK_WIDTH, 0);
                    const f32v2 tilePosWorld = worldPos + f32v2(0.5f, 0.5f);
                    const f32v2 offsetToTile = hitPosition2D - tilePosWorld;
                    if (glm::length2(offsetToTile) < brushSizeSq) {
                        editGrass(id, tileIndex, offsetToTile);
                    }
                }
            }

            // Notify grass to update
            for (Chunk* chunk : mWorld.mActiveChunks) {
                if (chunk->mChunkRenderData.mGrassLod) {
                    chunk->mChunkRenderData.mGrassLod->onDataChanged(f32v2(mHitResult.mPosition.x, mHitResult.mPosition.y), mCurrentBrushSettings->brushSize);
                }
            }
        }
    }
}

void WorldEditor::updateTileEdit() {

    static ChunkID prevChunkID;
    static TileIndex prevTileIndex;
    if (mHitResult.didHit() && vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
        ChunkID chunkID(f32v2(mHitResult.mPosition.x, mHitResult.mPosition.y));
        TileContainer& tileContainer = *mWorld.mWorldGrid.getChunk(chunkID).getTileContainer();
        TileIndex tileIndex = tileContainer.getTileIndexFromXYZOffset((ui32)mHitResult.mPosition.x % CHUNK_WIDTH, (ui32)mHitResult.mPosition.y % CHUNK_WIDTH, 0);
        Tile tile;
        const TileData& data = TileRepository::getTileData(mSelectedTile);

        // Make sure while mouse is held we arent spamming tiles in the same spot
        if (chunkID != prevChunkID || tileIndex != prevTileIndex) {
            prevChunkID = chunkID;
            prevTileIndex = tileIndex;
            tileContainer.addTile(tileIndex, data);

            if (mSelectedFloor == 0 && data.layer == TILE_LAYER_GROUND) {
                f32 height = mWorld.mWorldGrid.computeMinHeightAtTile(mHitResult.mPosition) + mGroundTileOffset;
                height = round(height);
                if (height == 0.0f) height = 1.0f;
                tileContainer.setTileGroundZPosition(tileIndex, height);
            }
        }
    }
    else {
        prevChunkID = 0;
    }
}

void WorldEditor::updateEntityEdit() {
    if (mHitResult.didHit() && vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT) && !mSelectedEntity.empty()) {
        mWorld.createEntity(mHitResult.mPosition, mSelectedEntity);
    }
}

void WorldEditor::updateCityEdit() {
    // Happens on mouse up
    if (mHitResult.didHit() && mCityEditState == CityEditState::CREATE) {
        f32v2 worldPos(mHitResult.mPosition.x, mHitResult.mPosition.y);
        TileHandle handle = mWorld.getTileHandleAtWorldPos(worldPos);
        mWorld.createCityAt(ui32v2(floor(worldPos.x), floor(worldPos.y)));
    }
}

void WorldEditor::updateBuildingEdit() {
    // Happens on mouse up
    if (mHitResult.didHit() && mBuildingEditState == BuildingEditState::CREATE) {
        f32v2 worldPos(mHitResult.mPosition.x, mHitResult.mPosition.y);
        TileHandle handle = mWorld.getTileHandleAtWorldPos(worldPos);
        ui32v2 createPos(floor(worldPos.x), floor(worldPos.y));

        // TODO: Unowned buildings?
        CityPlot plot;
        plot.aabb.pos = createPos;
        plot.aabb.dims = mPlotDims;
        plot.isFree = false;


        const f32 meanHeight = round(mWorld.getWorldGrid().computeMeanHeightAtAABB(plot.aabb));

        BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingRepository();
        std::unique_ptr<BuildingBlueprint> bp = BuildingBlueprintGenerator::generateBlueprintSync(buildingRepo, buildingRepo.getBuildingDef(mSelectedBuilding), 1.0f /*?*/, Cartesian::WEST, mPlotDims, createPos, INVALID_ENTITY, BuildingBlueprintFlags(0), meanHeight);

        CityBuilder::debugBuildInstant(*bp, mWorld);
    }
}

void WorldEditor::editVertex(HeightmapPatchID id, const ui32v2& vertPos, const f32v2& offsetToVertex) {
    
    // Read brush data
    f32 strength = getBrushStrengthAtPoint(offsetToVertex);

    if (strength > 0.001f) {
        switch (mTerrainEditState) {
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
        const f32 adjust = strength * mCurrentBrushSettings->brushStrength;
        mWorld.mWorldGrid.adjustHeightAt(id, vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + vertPos.x, adjust);

        // Debug render
        f32v2 chunkPos = id.getWorldPos();
        f32v2 dims(0.5f);
        f32v3 worldPos(chunkPos.x + vertPos.x * HEIGHTMAP_QUAD_SIZE - dims.x * 0.5f, chunkPos.y + vertPos.y * HEIGHTMAP_QUAD_SIZE - dims.y * 0.5f, mWorld.mWorldGrid.getHeightAtVert(id, vertPos) + adjust);
        DebugRenderer::drawWireQuad(worldPos, dims, color4(1.0f, 0.0f, 1.0f, abs(strength)), 3);
    }
}

void WorldEditor::editGrass(ChunkID id, TileIndex tileIndex, const f32v2& offsetToTile) {

    f32 strength = getBrushStrengthAtPoint(offsetToTile) * mCurrentBrushSettings->brushStrength;
    const f32 random = Random::getCachedRandomfSpecific(id.id * CHUNK_SIZE + tileIndex);
    if (random < strength) {
        if (mGrassEditState == GrassEditState::ADD) {
            mWorld.mWorldGrid.getChunk(id).setGrassAt(tileIndex, 1);
        }
        else {
            mWorld.mWorldGrid.getChunk(id).setGrassAt(tileIndex, 0);
        }
    }
}

f32 WorldEditor::getBrushStrengthAtPoint(const f32v2& brushOffsetToPoint)
{
    f32v2 offsetToCornerNormalized = (brushOffsetToPoint + f32v2(mCurrentBrushSettings->brushSize)) / f32v2(mCurrentBrushSettings->brushSize * 2.0f);
    if (offsetToCornerNormalized.x < 0.0f || offsetToCornerNormalized.y < 0.0f) {
        return 0.0f;
    }
    ui32v2 pixelPos = offsetToCornerNormalized * f32v2(mCurrentBrushSettings->activeBrush->dims.x, mCurrentBrushSettings->activeBrush->dims.y);
    if (pixelPos.x >= mCurrentBrushSettings->activeBrush->dims.x || pixelPos.y >= mCurrentBrushSettings->activeBrush->dims.y) {
        return 0.0f;
    }
    ui8 brushIntensity = mCurrentBrushSettings->activeBrush->data[pixelPos.y * mCurrentBrushSettings->activeBrush->dims.x + pixelPos.x];
    return (f32)brushIntensity / 255.0f;
}

void WorldEditor::setEditMode(WorldEditorEditMode mode) const {
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

