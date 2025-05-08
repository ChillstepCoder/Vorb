#include "stdafx.h"
#include "VisualLogger.h"

#include "rendering/RenderStats.h"
#include "rendering/mesh/mesher/builder/TextMeshBuilder.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h" // FOR SHARED
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/MaterialShaderDef.h"


#include "debugging/DebugMesh.h"
#include "options/DebugOptions.h"

#include <imgui.h>


#include <Vorb/graphics/GLProgram.h>

#include "rendering/gl/GL.h"

std::map<VisualLogCategory, std::vector<std::unique_ptr<VisualLog>>> VisualLogger::sVisualLogs;
std::vector<bool> VisualLogger::sFirstCategoryOpen = std::vector<bool>(e_count(VisualLogCategory), true);


std::mutex VisualLogger::sMutex;
AssetHandlePtr<MaterialShaderDef> sMaterialHandle;

VisualLog::VisualLog(const nString& name, VisualLogCategory category, bool isHiddenInUI) : mName(name), mCategory(category), mIsHiddenInUI(isHiddenInUI) {
    if (!sMaterialHandle) {
        sMaterialHandle = MaterialShaderRepository::get().getAssetHandle(CStrToken("text_billboard"));
    }
}

VisualLog::~VisualLog() {
    if (mLinesMesh.vao) {
        GL.glDeleteVertexArrays(1, &mLinesMesh.vao);
        GL.glDeleteBuffers(1, &mLinesMesh.vbo);
    }
    if (mQuadsMesh.vao) {
        GL.glDeleteVertexArrays(1, &mQuadsMesh.vao);
        GL.glDeleteBuffers(1, &mQuadsMesh.vbo);
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

void VisualLog::addWireTriangle(const f32v3 points[3], color4 color)
{
    mNumLines += 3;
    mRenderStepInfo.back().shapeCount += 3;
    for (int i = 0; i < 3; ++i) {
        VisualLogShape& newShape = mShapes.emplace_back();
        newShape.type = VisualLogShapeType::LINE; // TODO: WIRE_TRIANGLE?
        newShape.line.position1 = mRootPos + points[i];
        newShape.line.position2 = mRootPos + points[(i + 1) % 3];
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
    ++mRenderStepInfo.back().shapeCount;
    VisualLogShape& newShape = mShapes.emplace_back();
    newShape.type = VisualLogShapeType::TEXT;
    newShape.color = color;
    newShape.textIndex = mTextData.size();
    mTextData.emplace_back(VisualLogTextData{&font, str, rootPosition, offset2D, glyphHeight});
}

void VisualLog::addText(const nString& str, const f32v3& rootPosition, f32 glyphHeight, const f32v2& offset2D, color4 color) {
    assert(sDefaultFont);
    ++mRenderStepInfo.back().shapeCount;
    VisualLogShape& newShape = mShapes.emplace_back();
    newShape.type = VisualLogShapeType::TEXT;
    newShape.color = color;
    newShape.textIndex = mTextData.size();
    mTextData.emplace_back(VisualLogTextData{ sDefaultFont, str, rootPosition, offset2D, glyphHeight });
}

void VisualLog::finish() {
    if (mRenderStepInfo.size()) [[likely]] {
        mRenderStepInfo.back().end();
        mShapesToRender = mRenderStepInfo[0].shapeCount;
    }
    else {
        mShapesToRender = 0;
    }
    mSelectedRenderStep = 0;
    mFinishedBuilding = true;
    mDirtyRender = true;
}

void VisualLog::render(const f32v3 cameraPos, const f32m4& viewMatrix) {
    ASSERT_RENDER_THREAD();
    if (!mShapesToRender) {
        return;
    }

    const MaterialShaderDef* shaderDef = sMaterialHandle->tryGetLoadedAsset();
    if (!shaderDef) {
        return;
    }

    // Rebuild if needed
    if (mDirtyRender) {
        buildMesh();
    }

    // Quad meshes
    if (!sGlobalSimpleProgram.isCreated()) {
        initGlobalSimpleProgram();
    }


    sGlobalSimpleProgram.use();

    if (mQuadsMesh.vao) {
        glBindVertexArray(mQuadsMesh.vao);
        glUniformMatrix4fv(sGlobalSimpleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
        glUniform3fv(sGlobalSimpleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
        glDrawElements(GL_TRIANGLES, ((GLsizei)mQuadsMesh.numVerts / 4) * 6, GL_UNSIGNED_INT, nullptr);
        RenderStats::recordDrawCall(mQuadsMesh.numVerts / 4);
    }
    if (mLinesMesh.vao) {
        glBindVertexArray(mLinesMesh.vao);
        glUniformMatrix4fv(sGlobalSimpleProgram.getUniform("unWVP"), 1, GL_FALSE, &viewMatrix[0][0]);
        glUniform3fv(sGlobalSimpleProgram.getUniform("CameraPos"), 1, &cameraPos[0]);
        glDrawArrays(GL_LINES, 0, (GLsizei)mLinesMesh.numVerts);
        RenderStats::recordDrawCall(mLinesMesh.numVerts / 2);
    }

    // Render text
    if (mTextMesh.isValid()) {
        glDisable(GL_CULL_FACE); // TODO: Remove
        ui32 textureUnit;
        MaterialRenderer::bindMaterialShaderForRender(*shaderDef, &textureUnit);
        f32v3 offset = mRootPos - cameraPos;
        glUniform3fv(shaderDef->getUniform("unOffset"), 1, &offset.x);
        glUniform1i(shaderDef->getUniform("unFontTexture"), textureUnit);
        glBindTextureUnit(textureUnit, mTextData[0].font->mTexture);
        MeshDrawer::draw(mTextMesh.mGpuData);
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
    if (mRenderNShapes) {
        i = std::max(end - mNShapes, mRenderStepInfo[mSelectedRenderStep].startIndex);
    } else if (mRenderSingleStep) {
        i = mRenderStepInfo[mSelectedRenderStep].startIndex;
    }

    TextMeshBuilder textBuilder;
    
    // Build meshes
    for (; i < end && i < mShapes.size() /*Added second check cause we went out of bounds*/; ++i) {
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
        if (mLinesMesh.vao == 0) {
            glCreateVertexArrays(1, &mLinesMesh.vao);
        }
        else {
            // Refresh the vbo
            glDeleteBuffers(1, &mLinesMesh.vbo);
        }
        glCreateBuffers(1, &mLinesMesh.vbo);
        mLinesMesh.numVerts = lineVertices.size();
        mLinesMesh.type = DebugMeshType::LINES;
        glNamedBufferStorage(mLinesMesh.vbo, lineVertices.size() * sizeof(SimpleMeshVertex), lineVertices.data(), 0);
        glVertexArrayVertexBuffer(mLinesMesh.vao, 0, mLinesMesh.vbo, 0, sizeof(SimpleMeshVertex));

        glEnableVertexArrayAttrib(mLinesMesh.vao, 0);
        glVertexArrayAttribFormat(mLinesMesh.vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(SimpleMeshVertex, position));
        glVertexArrayAttribBinding(mLinesMesh.vao, 0, 0);
        glEnableVertexArrayAttrib(mLinesMesh.vao, 1);
        glVertexArrayAttribFormat(mLinesMesh.vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(SimpleMeshVertex, color));
        glVertexArrayAttribBinding(mLinesMesh.vao, 1, 0);

    }
    else if (mLinesMesh.vao) {
        glDeleteVertexArrays(1, &mLinesMesh.vao);
        glDeleteBuffers(1, &mLinesMesh.vbo);
        mLinesMesh.vbo = 0;
        mLinesMesh.vao = 0;
    }

    // Quads
    if (quadVertices.size()) {
        if (mQuadsMesh.vao == 0) {
            glCreateVertexArrays(1, &mQuadsMesh.vao);
        }
        else {
            // Refresh the vbo
            glDeleteBuffers(1, &mQuadsMesh.vbo);
        }
        glCreateBuffers(1, &mQuadsMesh.vbo);
        mQuadsMesh.numVerts = quadVertices.size();
        mQuadsMesh.type = DebugMeshType::QUADS;
        glNamedBufferStorage(mQuadsMesh.vbo, quadVertices.size() * sizeof(SimpleMeshVertex), quadVertices.data(), 0);
        glVertexArrayVertexBuffer(mQuadsMesh.vao, 0, mQuadsMesh.vbo, 0, sizeof(SimpleMeshVertex));
        glVertexArrayElementBuffer(mQuadsMesh.vao, ProceduralMeshBuilder::sQuadIboUI32);

        glEnableVertexArrayAttrib(mQuadsMesh.vao, 0);
        glVertexArrayAttribFormat(mQuadsMesh.vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(SimpleMeshVertex, position));
        glVertexArrayAttribBinding(mQuadsMesh.vao, 0, 0);
        glEnableVertexArrayAttrib(mQuadsMesh.vao, 1);
        glVertexArrayAttribFormat(mQuadsMesh.vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(SimpleMeshVertex, color));
        glVertexArrayAttribBinding(mQuadsMesh.vao, 1, 0);
    }
    else if (mQuadsMesh.vao) {
        glDeleteVertexArrays(1, &mQuadsMesh.vao);
        glDeleteBuffers(1, &mQuadsMesh.vbo);
        mQuadsMesh.vbo = 0;
        mQuadsMesh.vao = 0;
    }

    // Finish text
    textBuilder.finishMesh(mTextMesh, MeshDrawMode::STATIC);

    mDirtyRender = false;
}

VisualLog* VisualLogger::tryGetNewVisualLog(const nString& name, VisualLogCategory category, bool isHidden) {
    if (!sDebugOptions.mEnableVisualLogs) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(sMutex);
    VisualLog& newLog = *sVisualLogs[category].emplace_back(std::make_unique<VisualLog>(name, category, isHidden));
    return &newLog;
}

void VisualLogger::renderImgui(f32v3 cameraPos) {

    static std::pair<VisualLogCategory, ui32> sSelected = { {}, UINT32_MAX };
    static bool sAnimate = false;
    static int sAnimationSpeed = 32;
    static TickingTimer sAnimationTimer = TickingTimer(32);
    sAnimationTimer.startFrame();

    ImGui::Text("Logs");
    ImGui::Checkbox("Enable", &sDebugOptions.mEnableVisualLogs);
    if (ImGui::Checkbox("Animate", &sAnimate)) {
        sAnimationTimer.reset();
    }
    if (sAnimate) {
        if (ImGui::SliderInt("MS per tick", &sAnimationSpeed, 8, 512)) {
            sAnimationTimer.setMsPerTick(sAnimationSpeed);
        }
        ImGui::Separator();
    }

    std::lock_guard<std::mutex> lock(sMutex);
    for (auto&& it : sVisualLogs) {
        if (ImGui::TreeNode(ENUM_CSTR(VisualLogCategory, it.first))) {
            // Auto unhide the first category
            if (sFirstCategoryOpen[e_cast(it.first)]) {
                sFirstCategoryOpen[e_cast(it.first)] = false;
                unHideClosestLogInCategory(cameraPos, it.first);
            }
            ImGui::BeginTable("split1", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings);
            i32 hiddenCount = 0;
            auto& logs = it.second;
            for (size_t i = 0; i < logs.size(); ++i) {
                const VisualLog& log = *logs[i];
                if (log.mIsHiddenInUI) {
                    ++hiddenCount;
                    continue;
                }
                ImGui::PushID(999 + i);
                if (!log.mFinishedBuilding) {
                    ImGui::PopID();
                    continue;
                }

                ImGui::TableNextColumn();
                const std::pair<VisualLogCategory, ui32> prevSelected = sSelected;
                if (ImGui::RadioButton(log.mName.c_str(), sSelected.first == it.first && sSelected.second == (ui32)i)) {
                    sSelected.first = it.first;
                    sSelected.second = (ui32)i;
                    if (prevSelected.second != UINT32_MAX && prevSelected.second < sVisualLogs[prevSelected.first].size()) {
                        sVisualLogs[prevSelected.first][prevSelected.second]->mShouldRender = false;
                    }
                    logs[i]->mShouldRender = true;
                }
                ImGui::TableNextColumn();
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (hiddenCount) {
                if (ImGui::Button("Unhide Closest")) {
                    unHideClosestLogInCategory(cameraPos, it.first);
                }
                ImGui::Text("Hidden count %d", hiddenCount);
            }
            ImGui::TreePop();
        }
    }

    // Info about selected
    if (sSelected.second == UINT32_MAX) {
        return;
    }
    auto& logs = sVisualLogs[sSelected.first];
    if (sSelected.second < logs.size()) {
        VisualLog& log = *logs[sSelected.second];
        ImGui::Separator();
        ImGui::Text(log.mName.c_str());
        if (log.mUserString.size()) {
            ImGui::Text(log.mUserString.c_str());
        }
        // Collecct total time
        f32 total = 0.0f;
        for (auto&& step : log.mRenderStepInfo) {
            total += step.totalMs;
        }
        ImGui::Text("Total ms: %.2f", total);
        if (ImGui::Checkbox("Render single step", &log.mRenderSingleStep)) {
            log.mDirtyRender = true;
        }
        if (ImGui::Checkbox("Render N shapes", &log.mRenderNShapes)) {
            log.mDirtyRender = true;
        }
        if (log.mRenderNShapes) {
            if (ImGui::SliderInt("N", &log.mNShapes, 1, 16)) {
                log.mDirtyRender = true;
            }
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


        // Delete log at the end
        ImGui::PopID();
        ImGui::Separator();
        if (ImGui::Button("Delete")) {
            deleteLog(&log);
        }
        else {
            // Animation
            if (sAnimate) {
                int i = 0; // Make sure we have a max
                while (sAnimationTimer.tryTick() && (++i < 10)) {
                    log.mDirtyRender = true;
                    if ((int)log.mShapesToRender < (int)selected.shapeCount - 1) {
                        ++log.mShapesToRender;
                    }
                    else {
                        log.mShapesToRender = 0;
                        ++log.mSelectedRenderStep;
                        if (log.mSelectedRenderStep >= log.mRenderStepInfo.size()) {
                            log.mSelectedRenderStep = 0;
                        }
                    }
                }
            }
        }
    }
    
}

void VisualLogger::renderActiveLogs(const f32v3 cameraPos, const f32m4& viewMatrix) {

    std::lock_guard<std::mutex> lock(sMutex);
    for (auto&& it : sVisualLogs) {
        for (auto&& log : it.second) {
            if (log->mShouldRender) {
                log->render(cameraPos, viewMatrix);
            }
        }
    }
}

void VisualLogger::unHideClosestLogInCategory(f32v3 cameraPos, VisualLogCategory category) {
    std::vector<std::unique_ptr<VisualLog>>& logs = sVisualLogs[category];
    f32 closestDistSq = FLT_MAX;
    VisualLog* closestLog = nullptr;
    for (auto& log : logs) {
        if (log->mIsHiddenInUI) {
            f32v3 pos = log->mRootPos + log->mCameraDistanceCheckPosOffset;
            f32 distSq = glm::distance2(pos, cameraPos);
            if (distSq < closestDistSq) {
                closestDistSq = distSq;
                closestLog = log.get();
            }
        }
    }
    if (closestLog) {
        closestLog->mIsHiddenInUI = false;
    }
}

void VisualLogger::deleteLog(VisualLog* log) {
    assert(log);
    std::vector<std::unique_ptr<VisualLog>>& logs = sVisualLogs[log->mCategory];
    for (size_t i = 0; i < logs.size(); ++i) {
        if (logs[i].get() == log) {
            logs[i] = std::move(logs.back());
            logs.pop_back();
            return;
        }
    }
}
