#include "stdafx.h"
#include "CollisionEditor.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "ui/ImguiUtil.hpp"
#include "ui/imgui_controls/ObjectVector.h"
#include "ui/imgui_controls/EnumCombo.h"

bool CollisionEditor::updateAndRenderImguiControlsForShapeVector(std::vector<ModelColliderShape>& shapes) {
    return ImguiUtil::ObjectVector<ModelColliderShape>("Shapes", shapes,
        [](ModelColliderShape& o, ui32) {
            return CollisionEditor::updateAndRenderImguiControlsForShape(o);
        }, true /*resizable*/, ModelColliderShape()
    );
}

bool CollisionEditor::updateAndRenderImguiControlsForShape(ModelColliderShape& shape) {
    bool changed = false;

    changed |= ImguiUtil::EnumCombo<CollisionShapes>("Shape", shape.mShape, [](CollisionShapes val) {
        // Exclude non simple shape types
        return val != CollisionShapes::Mesh && val != CollisionShapes::Terrain && val != CollisionShapes::NONE;
    });
    static_assert(e_count(CollisionShapes) == 7, "Update this whole function if you add more collision shapes");

    const f32 contentAvailX = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(contentAvailX / 4.0f); // Width in pixels
    ImGui::Text("Position"); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##PositionX", &shape.mOffset.x, -1.0f, 1.0f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##PositionY", &shape.mOffset.y, -1.0f, 1.0f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##PositionZ", &shape.mOffset.z, -1.0f, 1.0f);
    ImGui::Text("Rotation"); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##RotationX", &shape.mEulerAngles.x, -180.0f, 180.0f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##RotationY", &shape.mEulerAngles.y, -180.0f, 180.0f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##RotationZ", &shape.mEulerAngles.z, -180.0f, 180.0f);
    ImGui::PopItemWidth();
    switch (shape.mShape) {
        case CollisionShapes::Capsule:
            changed |= ImGui::SliderFloat("Radius", &shape.mDims.x, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Height", &shape.mDims.y, 0.0f, 1.0f);
            break;
        case CollisionShapes::Cylinder:
            changed |= ImGui::SliderFloat("Radius", &shape.mDims.x, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Height", &shape.mDims.y, 0.0f, 1.0f);
            break;
        case CollisionShapes::Box:
            ImGui::PushItemWidth(contentAvailX / 4.0f); // Width in pixels
            ImGui::Text("Dims"); ImGui::SameLine();
            changed |= ImGui::SliderFloat("##Dims", &shape.mDims.x, 0.0f, 1.0f); ImGui::SameLine();
            changed |= ImGui::SliderFloat("##DimY", &shape.mDims.y, 0.0f, 1.0f); ImGui::SameLine();
            changed |= ImGui::SliderFloat("##DimZ", &shape.mDims.z, 0.0f, 1.0f);
            ImGui::PopItemWidth();
            break;
        case CollisionShapes::Sphere:
            changed |= ImGui::SliderFloat("Radius", &shape.mDims.x, 0.0f, 1.0f);
            break;
        default:
            ImGui::Text("INVALID SHAPE TYPE");
            break;

    }

    return changed;
}

void CollisionEditor::renderShapesInEditor(const std::vector<ModelColliderShape>& shapes) {
    for (auto& shape : shapes) {
        renderShapeInEditor(shape);
    }
}

void CollisionEditor::renderShapeInEditor(const ModelColliderShape& shape)
{
#ifdef JPH_DEBUG_RENDERER

#else
    __debugbreak(); // Should not be possible, we require the shapes generated from JPH
#endif
}
