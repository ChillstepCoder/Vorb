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
#include "editor/BrushRepository.h"

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

        if (mCurrentBrushSettings) {
            if (event.keyCode == VKEY_RIGHT) {
                mCurrentBrushSettings->brushSize = glm::min(mCurrentBrushSettings->brushSize + 0.2f, MAX_BRUSH_SIZE);
            }
            else if (event.keyCode == VKEY_LEFT) {
                mCurrentBrushSettings->brushSize = glm::max(mCurrentBrushSettings->brushSize - 0.2f, MIN_BRUSH_SIZE);
            }
            else if (event.keyCode == VKEY_1) {
                setEditMode(WorldEditorEditMode::TERRAIN);
            }
            else if (event.keyCode == VKEY_2) {
                setEditMode(WorldEditorEditMode::GRASS);
            }
        }
       
    });
}

void WorldEditor::update(const Camera3D& camera) {
    const f32v3& pickRay = sDebugOptions.mMousePickRay;

    if (!mCurrentBrushSettings || !mCurrentBrushSettings->activeBrush) {
        return;
    }

    mPickData = mWorld.getWorldGrid().pickTerrainFromCameraVector(camera, sDebugOptions.mMousePickRay);

    if (mEditMode == WorldEditorEditMode::TERRAIN) {
        updateTerrainEdit();
    }
    else if (mEditMode == WorldEditorEditMode::GRASS) {
        updateGrassEdit();
    }
}

void WorldEditor::renderBrushDecals (const Camera3D& camera) const {
    if (!mPickData.hit.didHit()) {
        return;
    }

    f32v3 origin = mPickData.hit.position - f32v3(mCurrentBrushSettings->brushSize, mCurrentBrushSettings->brushSize, 0.0f);
    f32v2 dims(mCurrentBrushSettings->brushSize * 2.0f);
    DebugRenderer::drawWireQuad(origin, dims, color4(0.0f, 0.0f, 1.0f, 0.9f));
}
// Use the manual it rocks
// https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
void WorldEditor::renderUI() const {
    const BrushRepository& brushRepo = mWorld.getResourceManager().getBrushRepository();

    constexpr float WINDOW_WIDTH = 400.0f;
    const float WINDOW_HEIGHT = mScreenDims.y;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    const ImVec2 buttonSize(WINDOW_WIDTH, 25);

    ImGui::Begin("World Editor", &sDebugOptions.mShowEditor, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    if (mEditMode == WorldEditorEditMode::TERRAIN) {
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(1.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(1.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(1.0f, 0.8f, 0.8f));
        ImGui::Button("Terrain"); ImGui::SameLine();
        ImGui::PopStyleColor(3);
    }
    else {
        if (ImGui::Button("Terrain")) {
            setEditMode(WorldEditorEditMode::TERRAIN);
        }
        ImGui::SameLine();
    }

    if (mEditMode == WorldEditorEditMode::GRASS) {
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(1.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(1.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(1.0f, 0.8f, 0.8f));
        ImGui::Button("Grass");
        ImGui::PopStyleColor(3);
    }
    else if (ImGui::Button("Grass")) {
        setEditMode(WorldEditorEditMode::GRASS);
    }

    ImGui::Text("Edit mode");
    if (mEditMode == WorldEditorEditMode::TERRAIN) {
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
    else if (mEditMode == WorldEditorEditMode::GRASS) {
        if (ImGui::RadioButton("Add", mGrassEditState == GrassEditState::ADD)) {
            mGrassEditState = GrassEditState::ADD;
        }
        if (ImGui::RadioButton("Remove", mGrassEditState == GrassEditState::REMOVE)) {
            mGrassEditState = GrassEditState::REMOVE;
        }
        ImGui::SliderFloat("Brush Size", &mGrassBrushSettings.brushSize, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Brush Strength", &mGrassBrushSettings.brushStrength, MIN_BRUSH_STRENGTH_GRASS, MAX_BRUSH_STRENGTH_GRASS, "%.3f", ImGuiSliderFlags_Logarithmic);
    }


    ImGui::NewLine();
    if (ImGui::CollapsingHeader("Brushes", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::vector<Brush>& brushes = brushRepo.getBrushes();
        ImGui::Indent();
        ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
        for (size_t i = 0; i < brushes.size(); ++i) {
            const Brush& brush = brushes[i];
            ImGui::TableNextColumn();
            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
            if (ImGui::RadioButton(brush.name.c_str(), mCurrentBrushSettings->brushId == i)) {
                mCurrentBrushSettings->brushId = i;
                mCurrentBrushSettings->activeBrush = &brush;
            }
            ImGui::TableNextColumn();
            ImGui::Image((ImTextureID)brush.texture, ImVec2(50.0f, 50.0f));
        }
        ImGui::EndTable();
        ImGui::Unindent();
    }

    ImGui::End();
}

void WorldEditor::updateTerrainEdit() {
    //PreciseTimer timer;
    // Pick terrain
    //std::cout << "TERRAIN PICK MS " << timer.stop() << std::endl;
    if (mPickData.hit.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
            PreciseTimer timer;
            // Edit the terrain with iteration
            const f32v2 hitPosition2D(mPickData.hit.position.x, mPickData.hit.position.y);
            const f32v2 worldPosBrushStart = hitPosition2D - f32v2(mCurrentBrushSettings->brushSize);
            const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(mCurrentBrushSettings->brushSize);
            const f32 brushSizeSq = SQ(mCurrentBrushSettings->brushSize);
            f32v2 worldPos;
            for (worldPos.y = worldPosBrushStart.y; worldPos.y <= worldPosBrushEnd.y + HEIGHTMAP_QUAD_SIZE; worldPos.y += HEIGHTMAP_QUAD_SIZE) {
                for (worldPos.x = worldPosBrushStart.x; worldPos.x <= worldPosBrushEnd.x + HEIGHTMAP_QUAD_SIZE; worldPos.x += HEIGHTMAP_QUAD_SIZE) {
                    ChunkID id(worldPos);
                    const f32v2 chunkWorldPos = id.getWorldPos();
                    const f32v2 offset = worldPos - chunkWorldPos;
                    const ui32v2 vertexPos = ui32v2(offset / (f32)HEIGHTMAP_QUAD_SIZE);
                    const f32v2 vertexPosWorld = f32v2(vertexPos) * (f32)HEIGHTMAP_QUAD_SIZE + chunkWorldPos;
                    const f32v2 offsetToVertex = hitPosition2D - vertexPosWorld;
                    if (glm::length2(offsetToVertex) < brushSizeSq) {
                        editVertex(id, vertexPos, offsetToVertex);
                    }
                }
            }

            // Notify all terrain stuff to update
            for (auto&& quadtree : mWorld.mTerrainTrees) {
                quadtree.onDataChanged(f32v2(mPickData.hit.position.x, mPickData.hit.position.y), mCurrentBrushSettings->brushSize);
            }
            for (Chunk* chunk : mWorld.mActiveChunks) {
                if (chunk->mChunkRenderData.mGrassLod) {
                    chunk->mChunkRenderData.mGrassLod->onDataChanged(f32v2(mPickData.hit.position.x, mPickData.hit.position.y), mCurrentBrushSettings->brushSize);
                }
            }

            std::cout << "TERRAIN FLOOD MS " << timer.stop() << std::endl;
        }
    }
}

void WorldEditor::updateGrassEdit() {

    if (mPickData.hit.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
            PreciseTimer timer;
            // Edit the terrain with iteration
            const f32v2 hitPosition2D(mPickData.hit.position.x, mPickData.hit.position.y);
            const f32v2 worldPosBrushStart = hitPosition2D - f32v2(mCurrentBrushSettings->brushSize);
            const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(mCurrentBrushSettings->brushSize);
            const f32 brushSizeSq = SQ(mCurrentBrushSettings->brushSize);
            f32v2 worldPos;
            for (worldPos.y = worldPosBrushStart.y; worldPos.y <= worldPosBrushEnd.y; worldPos.y += 1.0f) {
                for (worldPos.x = worldPosBrushStart.x; worldPos.x <= worldPosBrushEnd.x; worldPos.x += 1.0f) {
                    ChunkID id(worldPos);
                    const f32v2 chunkWorldPos = id.getWorldPos();
                    TileIndex tileIndex((ui32)worldPos.x % CHUNK_WIDTH, (ui32)worldPos.y % CHUNK_WIDTH);
                    const f32v2 tilePosWorld = chunkWorldPos + f32v2(tileIndex.getX() + 0.5f, tileIndex.getY() + 0.5f);
                    const f32v2 offsetToTile = hitPosition2D - tilePosWorld;
                    if (glm::length2(offsetToTile) < brushSizeSq) {
                        editGrass(id, tileIndex, offsetToTile);
                    }
                }
            }

            // Notify grass to update
            for (Chunk* chunk : mWorld.mActiveChunks) {
                if (chunk->mChunkRenderData.mGrassLod) {
                    chunk->mChunkRenderData.mGrassLod->onDataChanged(f32v2(mPickData.hit.position.x, mPickData.hit.position.y), mCurrentBrushSettings->brushSize);
                }
            }
        }
    }
}

void WorldEditor::editVertex(ChunkID id, const ui32v2& vertPos, const f32v2& offsetToVertex) {
    
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
        mWorld.mWorldGrid.adjustHeightAt(id, vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + vertPos.x, adjust);

        // Debug render
        f32v2 chunkPos = id.getWorldPos();
        f32v2 dims(0.5f);
        f32v3 worldPos(chunkPos.x + vertPos.x * HEIGHTMAP_QUAD_SIZE - dims.x * 0.5f, chunkPos.y + vertPos.y * HEIGHTMAP_QUAD_SIZE - dims.y * 0.5f, mWorld.mWorldGrid.getHeightAtVert(id, vertPos) + adjust);
        DebugRenderer::drawWireQuad(worldPos, dims, color4(1.0f, 0.0f, 1.0f, abs(strength)), 3);
    }
}

void WorldEditor::editGrass(ChunkID id, TileIndex tileIndex, const f32v2& offsetToTile) {

    f32 strength = getBrushStrengthAtPoint(offsetToTile) * mCurrentBrushSettings->brushStrength;
    const f32 random = Random::getCachedRandomfSpecific(id.id * CHUNK_SIZE + tileIndex.index);
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
        default:
            assert(false);
    }
    static_assert((int)WorldEditorEditMode::COUNT == 2, "Update");
}

