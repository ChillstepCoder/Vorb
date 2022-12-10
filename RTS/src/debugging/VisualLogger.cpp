#include "stdafx.h"
#include "VisualLogger.h"

#include "rendering/RenderStats.h"
#include "rendering/mesh/TextMeshBuilder.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"

#include "resources/ResourceManager.h"

#include "debugging/DebugMesh.h"
#include "options/DebugOptions.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GLProgram.h>

#include "rendering/gl/GL.h"

std::vector<std::unique_ptr<VisualLog>> VisualLogger::sVisualLogs;
std::mutex VisualLogger::sMutex;

VisualLog::VisualLog(const nString& name) : mName(name) {

}

VisualLog::~VisualLog() {
    if (mLinesMesh.vbo) {
        GL.glDeleteBuffers(1, &mLinesMesh.vbo);
        mLinesMesh.vbo = 0;
    }
    if (mQuadsMesh.vbo) {
        GL.glDeleteBuffers(1, &mQuadsMesh.vbo);
        mQuadsMesh.vbo = 0;
    }
}

void VisualLog::reserve(ui32 shapeCount) {
    mShapes.reserve(shapeCount);
}

void VisualLog::nextStep(const nString& stepName) {
    if (mRenderStepInfo.size()) {
        mRenderStepInfo.back().end();
    }
    mRenderStepInfo.emplace_back(VisualLogRenderStepInfo{ stepName, (ui32)mShapes.size(), 0u });
}

void VisualLog::addLineBetweenPoints(const f32v3& origin, const f32v3& end, const color4& color) {
    ++mNumLines;
    ++mRenderStepInfo.back().shapeCount;
    VisualLogShape& newShape = mShapes.emplace_back();
    newShape.type = VisualLogShapeType::LINE;
    newShape.line.position1 = mRootPos + origin;
    newShape.line.position2 = mRootPos + end;
    newShape.color = color;
}

void VisualLog::addWireQuad(const f32v3& origin, const f32v2& dims, color4 color) {
    mNumLines += 4;
    const f32v3 topRight = mRootPos + origin + f32v3(dims.x, dims.y, 0.0f);
    {
        ++mRenderStepInfo.back().shapeCount;
        VisualLogShape& newShape = mShapes.emplace_back();
        newShape.type = VisualLogShapeType::WIRE_QUAD;
        newShape.quad.position = mRootPos + origin;
        newShape.quad.dims = dims;
        newShape.color = color;
    }
   
}

void VisualLog::addFilledQuad(const f32v3& origin, const f32v2& dims, color4 color) {
    ++mNumQuads;
    ++mRenderStepInfo.back().shapeCount;
    VisualLogShape& newShape = mShapes.emplace_back();
    newShape.type = VisualLogShapeType::QUAD;
    newShape.quad.position = mRootPos + origin;
    newShape.quad.dims = dims;
    newShape.color = color;
}

void VisualLog::addCartesianArrow(const f32v3& center, f32 length, color4 color, Cartesian dir) {
    ++mNumArrows;
    ++mRenderStepInfo.back().shapeCount;
    VisualLogShape& newShape = mShapes.emplace_back();
    newShape.type = VisualLogShapeType::ARROW;
    newShape.color = color;
    const f32v3 worldCenter = mRootPos + center;
    const f32 halfLength = length * 0.5f;
    switch (dir) {
        case Cartesian::SOUTH:
            newShape.line.position1 = worldCenter + f32v3(0.0f, halfLength, 0.0f);
            newShape.line.position2 = worldCenter + f32v3(0.0f, -halfLength, 0.0f);
            break;
        case Cartesian::WEST:
            newShape.line.position1 = worldCenter + f32v3(halfLength, 0.0f, 0.0f);
            newShape.line.position2 = worldCenter + f32v3(-halfLength, 0.0f, 0.0f);
            break;
        case Cartesian::EAST:
            newShape.line.position1 = worldCenter + f32v3(-halfLength, 0.0f, 0.0f);
            newShape.line.position2 = worldCenter + f32v3(halfLength, 0.0f, 0.0f);
            break;
        case Cartesian::NORTH:
            newShape.line.position1 = worldCenter + f32v3(0.0f, -halfLength, 0.0f);
            newShape.line.position2 = worldCenter + f32v3(0.0f, halfLength, 0.0f);
            break;
        default:
            assert(false);
            break;

    }
}

void VisualLog::addText(const nString& str, const f32v3& rootPosition, const Font& font, f32 glyphHeight, const f32v2& offset2D, color4 color) {
    VisualLogShape& newShape = mShapes.emplace_back();
    newShape.type = VisualLogShapeType::TEXT;
    newShape.color = color;
    newShape.textIndex = mTextData.size();
    mTextData.emplace_back(VisualLogTextData{&font, str, rootPosition, offset2D, glyphHeight});
}

void VisualLog::finish() {
    assert(mRenderStepInfo.size());
    mRenderStepInfo.back().end();
    mSelectedRenderStep = 0;
    mShapesToRender = mRenderStepInfo[0].shapeCount;
    mFinishedBuilding = true;
    mDirtyRender = true;
}

void VisualLog::render(const f32v3& cameraPos, const f32m4& viewMatrix) {
    assert(IS_GAME_THREAD());

    // Rebuild if needed
    if (mDirtyRender) {
        buildMesh();
    }

    // Quad meshes
    if (!sGlobalSimpleProgram.isCreated()) {
        initGlobalSimpleProgram();
    }

    // Make sure we dont modify state
    glBindVertexArray(0);

    sGlobalSimpleProgram.use();
    sGlobalSimpleProgram.enableVertexAttribArrays();

    if (mQuadsMesh.vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, mQuadsMesh.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glVertexAttribPointer(sGlobalSimpleProgram.getAttribute("vPosition"), 3, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
        glVertexAttribPointer(sGlobalSimpleProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
        glUniformMatrix4fv(sGlobalSimpleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
        glUniform3fv(sGlobalSimpleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
        glDrawArrays(GL_QUADS, 0, (GLsizei)mQuadsMesh.numVerts);
        RenderStats::recordDrawCall(mQuadsMesh.numVerts / 4);
    }
    if (mLinesMesh.vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, mLinesMesh.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glVertexAttribPointer(sGlobalSimpleProgram.getAttribute("vPosition"), 3, GL_FLOAT, GL_FALSE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, position));
        glVertexAttribPointer(sGlobalSimpleProgram.getAttribute("vColor"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SimpleMeshVertex), offsetptr(SimpleMeshVertex, color));
        glUniformMatrix4fv(sGlobalSimpleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
        glUniform3fv(sGlobalSimpleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
        glDrawArrays(GL_LINES, 0, (GLsizei)mLinesMesh.numVerts);
        RenderStats::recordDrawCall(mLinesMesh.numVerts / 2);
    }

    sGlobalSimpleProgram.disableVertexAttribArrays();
    sGlobalSimpleProgram.unuse();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Render text
    if (mTextMesh.isValid()) {
        glDisable(GL_CULL_FACE);
        const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialManager();
        const MaterialShader* material = materialManager.getMaterialShader("text_billboard");
        MaterialRenderer::bindMaterialForRender(*material);
        f32v3 offset = mRootPos - cameraPos;
        glUniform3fv(material->getUniform("unOffset"), 1, &offset.x);
        mTextMesh.draw();
    }
}

void VisualLog::buildMesh() {
    std::vector<SimpleMeshVertex> lineVertices;
    std::vector<SimpleMeshVertex> quadVertices;
    // Reserve maximum size of mesh
    lineVertices.reserve(mNumLines * 2 + mNumArrows * 6);
    quadVertices.reserve(mNumQuads * 4);

    ui32 i = 0;
    const ui32 end = mRenderStepInfo[mSelectedRenderStep].startIndex + mShapesToRender;
    if (mRenderSingleShape) {
        i = end - 1;
    } else if (mRenderSingleStep) {
        i = mRenderStepInfo[mSelectedRenderStep].startIndex;
    }

    TextMeshBuilder textBuilder;
    
    // Build meshes
    for (; i < end; ++i) {
        const VisualLogShape& shape = mShapes[i];
        switch (shape.type) {
            case VisualLogShapeType::LINE: {
                SimpleMeshVertex& v1 = lineVertices.emplace_back();
                v1.position = shape.line.position1;
                v1.color = shape.color;
                SimpleMeshVertex& v2 = lineVertices.emplace_back();
                v2.position = shape.line.position2;
                v2.color = shape.color;
                break;
            }
            case VisualLogShapeType::QUAD: {
                const SimpleQuad& q = shape.quad;
                SimpleMeshVertex& v1 = quadVertices.emplace_back();
                v1.position = q.position;
                v1.color = shape.color;
                SimpleMeshVertex& v2 = quadVertices.emplace_back();
                v2.position = q.position + f32v3(q.dims.x, 0.0f, 0.0f);
                v2.color = shape.color;
                SimpleMeshVertex& v3 = quadVertices.emplace_back();
                v3.position = q.position + f32v3(q.dims.x, q.dims.y, 0.0f);
                v3.color = shape.color;
                SimpleMeshVertex& v4 = quadVertices.emplace_back();
                v4.position = q.position + f32v3(0.0f, q.dims.y, 0.0f);
                v4.color = shape.color;
                break;
            }
            case VisualLogShapeType::WIRE_QUAD: {
                const f32v3 p1 = shape.quad.position;
                const f32v3 p2 = p1 + f32v3(shape.quad.dims.x, 0.0f, 0.0f);
                const f32v3 p3 = p1 + f32v3(shape.quad.dims.x, shape.quad.dims.y, 0.0f);
                const f32v3 p4 = p1 + f32v3(0.0f, shape.quad.dims.y, 0.0f);
                // Four lines
                {
                    SimpleMeshVertex& v1 = lineVertices.emplace_back();
                    v1.position = p1;
                    v1.color = shape.color;
                    SimpleMeshVertex& v2 = lineVertices.emplace_back();
                    v2.position = p2;
                    v2.color = shape.color; 
                }
                {
                    SimpleMeshVertex& v1 = lineVertices.emplace_back();
                    v1.position = p2;
                    v1.color = shape.color;
                    SimpleMeshVertex& v2 = lineVertices.emplace_back();
                    v2.position = p3;
                    v2.color = shape.color;
                }
                {
                    SimpleMeshVertex& v1 = lineVertices.emplace_back();
                    v1.position = p3;
                    v1.color = shape.color;
                    SimpleMeshVertex& v2 = lineVertices.emplace_back();
                    v2.position = p4;
                    v2.color = shape.color;
                }
                {
                    SimpleMeshVertex& v1 = lineVertices.emplace_back();
                    v1.position = p4;
                    v1.color = shape.color;
                    SimpleMeshVertex& v2 = lineVertices.emplace_back();
                    v2.position = p1;
                    v2.color = shape.color;
                }
                break;
            }
            case VisualLogShapeType::ARROW: {
                SimpleMeshVertex& v1 = lineVertices.emplace_back();
                v1.position = shape.line.position1;
                v1.color = shape.color;
                SimpleMeshVertex& v2 = lineVertices.emplace_back();
                v2.position = shape.line.position2;
                v2.color = shape.color;
                // Arrow parts
                const f32v3 offset = v1.position - v2.position;
                const f32v2 l1 = MathUtil::RotateVector(offset.x, offset.y, 30.0f) * 0.2f;
                const f32v2 l2 = MathUtil::RotateVector(offset.x, offset.y, -30.0f) * 0.2f;
                SimpleMeshVertex& v3 = lineVertices.emplace_back();
                v3.position = shape.line.position2;
                v3.color = shape.color;
                SimpleMeshVertex& v4 = lineVertices.emplace_back();
                v4.position = shape.line.position2 + f32v3(l1.x, l1.y, 0.0f);
                v4.color = shape.color;
                SimpleMeshVertex& v5 = lineVertices.emplace_back();
                v5.position = shape.line.position2;
                v5.color = shape.color;
                SimpleMeshVertex& v6 = lineVertices.emplace_back();
                v6.position = shape.line.position2 + f32v3(l2.x, l2.y, 0.0f);
                v6.color = shape.color;
                break;
            }
            case VisualLogShapeType::TEXT: {
                VisualLogTextData& data = mTextData[shape.textIndex];
                textBuilder.addString(data.str, data.rootPos, *data.font, data.glyphHeight, data.offset2D, TextAlign::CENTER);
                break;
            }
            default:
                assert(false);
                break;
        }
    }
    static_assert(e_cast(VisualLogShapeType::COUNT) == 5);

    // Lines
    if (lineVertices.size()) {
        if (mLinesMesh.vbo == 0) {
            glGenBuffers(1, &mLinesMesh.vbo);
        }
        mLinesMesh.numVerts = lineVertices.size();
        mLinesMesh.type = DebugMeshType::LINES;
        glBindBuffer(GL_ARRAY_BUFFER, mLinesMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data());
    }
    else if (mLinesMesh.vbo) {
        glDeleteBuffers(1, &mLinesMesh.vbo);
        mLinesMesh.vbo = 0;
    }

    // Quads
    if (quadVertices.size()) {
        if (mQuadsMesh.vbo == 0) {
            glGenBuffers(1, &mQuadsMesh.vbo);
        }
        mQuadsMesh.numVerts = quadVertices.size();
        mQuadsMesh.type = DebugMeshType::QUADS;
        glBindBuffer(GL_ARRAY_BUFFER, mQuadsMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(SimpleMeshVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, quadVertices.size() * sizeof(SimpleMeshVertex), quadVertices.data());
    }
    else if (mQuadsMesh.vbo) {
        glDeleteBuffers(1, &mQuadsMesh.vbo);
        mQuadsMesh.vbo = 0;
    }

    // Finish text
    textBuilder.finishMesh(mTextMesh, MeshDrawMode::STATIC);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    mDirtyRender = false;
}

VisualLog* VisualLogger::tryGetNewVisualLog(const nString& name) {
    if (!sDebugOptions.mEnableVisualLogs) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(sMutex);
    VisualLog& newLog = *sVisualLogs.emplace_back(std::make_unique<VisualLog>(name));
    return &newLog;
}

void VisualLogger::renderImgui() {

    static ui32 sSelected = UINT32_MAX;

    ImGui::Text("Logs");
    ImGui::Checkbox("Enable", &sDebugOptions.mEnableVisualLogs);

    ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);

    std::lock_guard<std::mutex> lock(sMutex);
    for (size_t i = 0; i < sVisualLogs.size(); ++i) {
        ImGui::PushID(999 + i);
        const VisualLog& log = *sVisualLogs[i];
        if (!log.mFinishedBuilding) {
            ImGui::PopID();
            continue;
        }

        ImGui::TableNextColumn();
        const ui32 prevSelected = sSelected;
        if (ImGui::RadioButton(log.mName.c_str(), sSelected == (ui32)i)) {
            sSelected = (ui32)i;
            if (prevSelected < sVisualLogs.size()) {
                sVisualLogs[prevSelected]->mShouldRender = false;
            }
            sVisualLogs[sSelected]->mShouldRender = true;
        }
        ImGui::TableNextColumn();
        ImGui::PopID();
    }
    ImGui::EndTable();

    // Info about selected
    if (sSelected < sVisualLogs.size()) {
        VisualLog& log = *sVisualLogs[sSelected];
        ImGui::Separator();
        ImGui::Text(log.mName.c_str());
        // Collecct total time
        f32 total = 0.0f;
        for (auto&& step : log.mRenderStepInfo) {
            total += step.totalMs;
        }
        ImGui::Text("Total ms: %.2f", total);
        if (ImGui::Checkbox("Render single step", &log.mRenderSingleStep)) {
            log.mDirtyRender = true;
        }
        if (ImGui::Checkbox("Render single shape", &log.mRenderSingleShape)) {
            log.mDirtyRender = true;
        }
        ImGui::Text("%s", log.mRenderStepInfo[log.mSelectedRenderStep].stepName.c_str());
        if (log.mRenderStepInfo.size() > 1) {
            if (ImGui::Button("-1")) {
                if (log.mSelectedRenderStep > 0) {
                    --log.mSelectedRenderStep;
                    log.mShapesToRender = log.mRenderStepInfo[log.mSelectedRenderStep].shapeCount;
                    log.mDirtyRender = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+1")) {
                if (log.mSelectedRenderStep < log.mRenderStepInfo.size() - 1) {
                    ++log.mSelectedRenderStep;
                    log.mShapesToRender = log.mRenderStepInfo[log.mSelectedRenderStep].shapeCount;
                    log.mDirtyRender = true;
                }
            }
        }
        if (ImGui::SliderInt("Step", &log.mSelectedRenderStep, 0, log.mRenderStepInfo.size() - 1)) {
            log.mShapesToRender = log.mRenderStepInfo[log.mSelectedRenderStep].shapeCount;
            log.mDirtyRender = true;
        }

        ImGui::Separator();
        VisualLogRenderStepInfo& selected = log.mRenderStepInfo[log.mSelectedRenderStep];
        ImGui::PushID(9999);
        ImGui::Text("Step ms: %.2f", selected.totalMs);
        if (selected.shapeCount > 1) {
            if (ImGui::Button("-1")) {
                if (log.mShapesToRender > 0) {
                    --log.mShapesToRender;
                    log.mDirtyRender = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+1")) {
                if (log.mShapesToRender < selected.shapeCount) {
                    ++log.mShapesToRender;
                    log.mDirtyRender = true;
                }
            }
            if (ImGui::SliderInt("Shapes", &log.mShapesToRender, 0, selected.shapeCount)) {
                log.mDirtyRender = true;
            }
        }
        ImGui::PopID();
        ImGui::Separator();
        if (ImGui::Button("Delete")) {
            deleteLog(&log);
        }
    }
    ImGui::Separator();
}

void VisualLogger::renderActiveLogs(const f32v3& cameraPos, const f32m4& viewMatrix) {

    std::lock_guard<std::mutex> lock(sMutex);
    for (auto&& log : sVisualLogs) {
        if (log->mShouldRender) {
            log->render(cameraPos, viewMatrix);
        }
    }
}

void VisualLogger::deleteLog(VisualLog* log) {

    for (size_t i = 0; i < sVisualLogs.size(); ++i) {
        if (sVisualLogs[i].get() == log) {
            sVisualLogs[i] = std::move(sVisualLogs.back());
            sVisualLogs.pop_back();
            return;
        }
    }
}
