#include "stdafx.h"
#include "WorldEditor.h"

#include "World.h"
#include "world/WorldGrid.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "world/Chunk.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "options/DebugOptions.h"
#include "DebugRenderer.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/InputDispatcher.h>

constexpr f32 MIN_BRUSH_SIZE = 0.1f;
constexpr f32 MAX_BRUSH_SIZE = 50.0f;
constexpr f32 MIN_BRUSH_STRENGTH = 0.01f;
constexpr f32 MAX_BRUSH_STRENGTH = 2.0f;

WorldEditor::WorldEditor(World& world, const f32v2& screenDims) : mWorld(world), mScreenDims(screenDims) {

    // Inputs
    vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {

        if (!sDebugOptions.mShowEditor || mEditState == WorldEditorEditState::NONE) return;

        // View toggle
        if (event.keyCode == VKEY_RIGHT) {
            mBrushSize = glm::min(mBrushSize + 0.2f, MAX_BRUSH_SIZE);
        }
        else if (event.keyCode == VKEY_LEFT) {
            mBrushSize = glm::max(mBrushSize - 0.2f, MIN_BRUSH_SIZE);
        }
    });
}

void WorldEditor::update(const Camera3D& camera) {
    const f32v3& pickRay = sDebugOptions.mMousePickRay;

    //PreciseTimer timer;
    // Pick terrain
    mPickData = mWorld.getWorldGrid().pickTerrainFromCameraVector(camera, sDebugOptions.mMousePickRay);
    //std::cout << "TERRAIN PICK MS " << timer.stop() << std::endl;
    if (mEditState != WorldEditorEditState::NONE && mPickData.hit.didHit()) {
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
            PreciseTimer timer;
            // Edit the terrain with iteration
            const f32v2 hitPosition2D(mPickData.hit.position.x, mPickData.hit.position.y);
            const f32v2 worldPosBrushStart = hitPosition2D - f32v2(mBrushSize);
            const f32v2 worldPosBrushEnd = hitPosition2D + f32v2(mBrushSize);
            const f32 brushSizeSq = SQ(mBrushSize);
            f32v2 worldPos;
            for (worldPos.y = worldPosBrushStart.y; worldPos.y <= worldPosBrushEnd.y + HEIGHTMAP_QUAD_SIZE; worldPos.y += HEIGHTMAP_QUAD_SIZE) {
                for (worldPos.x = worldPosBrushStart.x; worldPos.x <= worldPosBrushEnd.x + HEIGHTMAP_QUAD_SIZE; worldPos.x += HEIGHTMAP_QUAD_SIZE) {
                    ChunkID id(worldPos);
                    const f32v2 chunkWorldPos = id.getWorldPos();
                    const f32v2 offset = worldPos - chunkWorldPos;
                    const ui32v2 vertexPos = ui32v2(offset / (f32)HEIGHTMAP_QUAD_SIZE);
                    const f32v2 vertexPosWorld = f32v2(vertexPos) * (f32)HEIGHTMAP_QUAD_SIZE + chunkWorldPos;
                    const f32v2 offsetToHit = vertexPosWorld - hitPosition2D;
                    if (glm::length2(offsetToHit) < brushSizeSq) {
                        editVertex(id, vertexPos, offsetToHit);
                    }
                }
            }

            // Notify all terrain stuff to update
            for (auto&& quadtree : mWorld.mTerrainTrees) {
                quadtree.onDataChanged(f32v2(mPickData.hit.position.x, mPickData.hit.position.y), mBrushSize);
            }
            for (Chunk* chunk : mWorld.mActiveChunks) {
                if (chunk->mChunkRenderData.mGrassLod) {
                    chunk->mChunkRenderData.mGrassLod->onDataChanged(f32v2(mPickData.hit.position.x, mPickData.hit.position.y), mBrushSize);
                }
            }

            std::cout << "TERRAIN FLOOD MS " << timer.stop() << std::endl;
        }
    }
}

void WorldEditor::renderBrushDecals (const Camera3D& camera) const {
    if (mEditState == WorldEditorEditState::NONE || !mPickData.hit.didHit()) {
        return;
    }

    f32v3 origin = mPickData.hit.position - f32v3(mBrushSize, mBrushSize, 0.0f);
    f32v2 dims(mBrushSize * 2.0f);
    DebugRenderer::drawWireQuad(origin, dims, color4(0.0f, 0.0f, 1.0f, 0.9f));
}

void WorldEditor::renderUI() const {
    constexpr float WINDOW_WIDTH = 400.0f;
    const float WINDOW_HEIGHT = mScreenDims.y;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    const ImVec2 buttonSize(WINDOW_WIDTH, 25);

    ImGui::Begin("World Editor", &sDebugOptions.mShowEditor, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    ImGui::Text("Edit mode");
    ImGui::BeginGroup();
    if (ImGui::RadioButton("None", mEditState == WorldEditorEditState::NONE)) {
        mEditState = WorldEditorEditState::NONE;
    }
    if (ImGui::RadioButton("Lower", mEditState == WorldEditorEditState::LOWER_TERRAIN)) {
        mEditState = WorldEditorEditState::LOWER_TERRAIN;
    }
    if (ImGui::RadioButton("Raise", mEditState == WorldEditorEditState::RAISE_TERRAIN)) {
        mEditState = WorldEditorEditState::RAISE_TERRAIN;
    }
    if (ImGui::RadioButton("Flatten", mEditState == WorldEditorEditState::FLATTEN_TERRAIN)) {
        mEditState = WorldEditorEditState::FLATTEN_TERRAIN;
    }
    ImGui::EndGroup();
    ImGui::NewLine();
    ImGui::Text("Brush");
    ImGui::BeginGroup();
    if (ImGui::RadioButton("Smooth Box", mBrushType == WorldEditorBrushType::SMOOTH_BOX)) {
        mBrushType = WorldEditorBrushType::SMOOTH_BOX;
    }
    if (ImGui::RadioButton("Hard Box", mBrushType == WorldEditorBrushType::HARD_BOX)) {
        mBrushType = WorldEditorBrushType::HARD_BOX;
    }

    if (ImGui::RadioButton("Smooth Circle", mBrushType == WorldEditorBrushType::SMOOTH_CIRCLE)) {
        mBrushType = WorldEditorBrushType::SMOOTH_CIRCLE;
    }
    if (ImGui::RadioButton("Hard Circle", mBrushType == WorldEditorBrushType::HARD_CIRCLE)) {
        mBrushType = WorldEditorBrushType::HARD_CIRCLE;
    }
    ImGui::EndGroup();
    ImGui::SliderFloat("Brush Size", (float*)&mBrushSize, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Brush Strength", (float*)&mBrushStrength, MIN_BRUSH_STRENGTH, MAX_BRUSH_STRENGTH, "%.3f", ImGuiSliderFlags_Logarithmic);

    ImGui::End();
}

void WorldEditor::editVertex(ChunkID id, const ui32v2& vertPos, const f32v2& offsetToBrush)
{
    
    f32 strength = 1.0f;

    switch (mBrushType) {
        case WorldEditorBrushType::SMOOTH_BOX: {
            f32 distance = glm::min(abs(offsetToBrush.x), abs(offsetToBrush.y));
            strength *= (1.0f - distance / mBrushSize);
            break;
        }
        case WorldEditorBrushType::HARD_BOX: {
            break;
        }
        case WorldEditorBrushType::SMOOTH_CIRCLE: {
            f32 distance = glm::length(offsetToBrush);
            strength *= 1.0f - distance / mBrushSize;
            break;
        }
        case WorldEditorBrushType::HARD_CIRCLE: {
            f32 distance = glm::length(offsetToBrush);
            if (distance > mBrushSize) {
                strength = 0;
            }
            break;
        }
        default:
            assert(false);
            break;
        
    }
    static_assert((int)WorldEditorBrushType::COUNT == 4, "Update for new brush");

    if (strength > 0.001f) {
        switch (mEditState) {
            case WorldEditorEditState::RAISE_TERRAIN:
                break;
            case WorldEditorEditState::LOWER_TERRAIN:
                strength = -strength;
                break;
            case WorldEditorEditState::FLATTEN_TERRAIN:
                break;
            default:
                assert(false);
                break;
        }
        static_assert((int)WorldEditorEditState::COUNT == 4, "Update for new edit type");
        const f32 adjust = strength * mBrushStrength;
        mWorld.mWorldGrid.adjustHeightAt(id, vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + vertPos.x, adjust);

        // Debug render
        f32v2 chunkPos = id.getWorldPos();
        f32v2 dims(0.5f);
        f32v3 worldPos(chunkPos.x + vertPos.x * HEIGHTMAP_QUAD_SIZE - dims.x * 0.5f, chunkPos.y + vertPos.y * HEIGHTMAP_QUAD_SIZE - dims.y * 0.5f, mWorld.mWorldGrid.getHeightAtVert(id, vertPos) + adjust);
        DebugRenderer::drawWireQuad(worldPos, dims, color4(1.0f, 0.0f, 1.0f, abs(strength)), 3);
    }
}
