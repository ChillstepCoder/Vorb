#include "stdafx.h"
#include "CollisionEditor.h"

#include <imgui.h>
#include "ui/ImguiUtil.hpp"
#include "ui/imgui_controls/ObjectVector.h"
#include "ui/imgui_controls/EnumCombo.h"
#include "ui/editor/ImguiAssetThumbnails.h"
#include "ui/UIContext.h"

#include "camera/SimpleCamera.h"
#include "camera/Camera3D.h"

#include "resources/ModelRepository.h"

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include "physics/PhysicsDebugRenderer.h"
#include <vorb/graphics/DepthState.h>

#endif

static const f32v2 THUMBNAIL_SIZE = f32v2(50.0f);
static bool sShowCollision = true;
static bool sDisableDepth = false;
static bool sWireframe = true;
static std::unique_ptr<ImguiUtil::AssetSelectorPopup> sModelSelectorPopup;
static std::vector<f32v3> sCollisionColors;

void addNewColorsIfNeeded(size_t count) {
    while (sCollisionColors.size() < count) {
        sCollisionColors.push_back(f32v3((rand() % 255) / 255.f, (rand() % 255) / 255.f, (rand() % 255) / 255.f));
    }
}

bool CollisionEditor::updateAndRenderImguiControlsForShapeVector(std::vector<ModelColliderShape>& shapes) {
    ImGui::Checkbox("Show Collision", &sShowCollision);
    ImGui::Checkbox("Disable Depth", &sDisableDepth);
    ImGui::Checkbox("Wireframe", &sWireframe);
    if (ImGui::Button("Copy From Other Model")) {
        sModelSelectorPopup = std::make_unique<ImguiUtil::AssetSelectorPopup>(ModelRepository::get().getAssetRegistry());
        sModelSelectorPopup->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction(AssetType::Model), THUMBNAIL_SIZE);
    }

    if (sModelSelectorPopup) {
        if (sModelSelectorPopup->updateAndRender(UIContext::getWindowDims().y * 0.9f)) {
            StrToken result = sModelSelectorPopup->getResult().mName;
            if (result.isValid()) {
                const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(sModelSelectorPopup->getResult().mDescriptor.id);
                shapes = modelDef.mColliderData.mSubShapes;
                sModelSelectorPopup.reset();
                return true;
            }
            sModelSelectorPopup.reset();
        }
        return false;
    }
    else {
        addNewColorsIfNeeded(shapes.size());
        return ImguiUtil::ObjectVector<ModelColliderShape, true>("Shapes", shapes,
            [](ModelColliderShape& o, ui32 i) {
                ImGui::ColorEdit3("Color", &sCollisionColors[i].x);
                return CollisionEditor::updateAndRenderImguiControlsForShape(o);
            }, true /*resizable*/, ModelColliderShape()
        );
    }
}

bool CollisionEditor::updateAndRenderImguiControlsForShape(ModelColliderShape& shape) {
    bool changed = false;

    changed |= ImguiUtil::EnumCombo<CollisionShapes>("Shape", shape.mShape, [](CollisionShapes val) {
        // Exclude non simple shape types
        return val != CollisionShapes::Mesh && val != CollisionShapes::Terrain && val != CollisionShapes::NONE;
    });
    if (changed) {
        // If we swapped to sphere, no rotate
        if (shape.mShape == CollisionShapes::Sphere) {
            shape.mEulerAngles = f32v3(0.0f);
        }
    }
    static_assert(e_count(CollisionShapes) == 7, "Update this whole function if you add more collision shapes");

    const f32 contentAvailX = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(contentAvailX / 4.0f); // Width in pixels
    ImGui::Text("Position"); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##PositionX", &shape.mOffset.x, -8.0f, 8.0f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##PositionY", &shape.mOffset.y, -8.0f, 8.0f); ImGui::SameLine();
    changed |= ImGui::SliderFloat("##PositionZ", &shape.mOffset.z, -8.0f, 8.0f);
    if (shape.mShape != CollisionShapes::Sphere) {
        ImGui::Text("Rotation"); ImGui::SameLine();
        f32v3 shapeEulerDeg = glm::degrees(shape.mEulerAngles);
        changed |= ImGui::SliderFloat("##RotationX", &shapeEulerDeg.x, -180.f, 180.f, "%1f"); ImGui::SameLine();
        changed |= ImGui::SliderFloat("##RotationY", &shapeEulerDeg.y, -180.f, 180.f, "%1f"); ImGui::SameLine();
        changed |= ImGui::SliderFloat("##RotationZ", &shapeEulerDeg.z, -180.f, 180.f, "%1f");
        shape.mEulerAngles = glm::radians(shapeEulerDeg);
    }
    ImGui::PopItemWidth();
    switch (shape.mShape) {
        case CollisionShapes::Capsule:
            changed |= ImGui::SliderFloat("Radius", &shape.mHalfDims.x, 0.0f, 8.0f);
            changed |= ImGui::SliderFloat("Height", &shape.mHalfDims.y, 0.0f, 8.0f);
            break;
        case CollisionShapes::Cylinder:
            changed |= ImGui::SliderFloat("Radius", &shape.mHalfDims.x, 0.0f, 8.0f);
            changed |= ImGui::SliderFloat("Height", &shape.mHalfDims.y, 0.0f, 8.0f);
            break;
        case CollisionShapes::Box:
            ImGui::PushItemWidth(contentAvailX / 4.0f); // Width in pixels
            ImGui::Text("Dims"); ImGui::SameLine();
            changed |= ImGui::SliderFloat("##Dims", &shape.mHalfDims.x, 0.0f, 8.0f); ImGui::SameLine();
            changed |= ImGui::SliderFloat("##DimY", &shape.mHalfDims.y, 0.0f, 8.0f); ImGui::SameLine();
            changed |= ImGui::SliderFloat("##DimZ", &shape.mHalfDims.z, 0.0f, 8.0f);
            ImGui::PopItemWidth();
            break;
        case CollisionShapes::Sphere:
            changed |= ImGui::SliderFloat("Radius", &shape.mHalfDims.x, 0.0f, 1.0f);
            break;
        default:
            ImGui::Text("INVALID SHAPE TYPE");
            break;

    }

    shape.mHalfDims = glm::max(shape.mHalfDims, f32v3(0.001f));

    return changed;
}

void CollisionEditor::renderShapesInEditor(const std::vector<ModelColliderShape>& shapes, const SimpleCamera& camera) {
    addNewColorsIfNeeded(shapes.size());
    int i = 0;
    for (auto& shape : shapes) {
        renderShapeInEditor(shape, camera, sCollisionColors[i]);
        ++i;
    }
}

void CollisionEditor::renderShapeInEditor(const ModelColliderShape& shape, const SimpleCamera& camera, f32v3 color) {
#ifdef JPH_DEBUG_RENDERER
    if (!sShowCollision) {
        return;
    }
    if (!sPhysicsDebugRenderer) {
        sPhysicsDebugRenderer = std::make_unique<PhysicsDebugRenderer>();
    }

    if (sDisableDepth) {
        vg::DepthState::NONE.set();
    }

    Camera3D cameraCopy;
    cameraCopy.copyFromSimpleCamera(camera);
    sPhysicsDebugRenderer->PrepareFrame(cameraCopy);

    const JPH::DVec3 shapePos = JPH::DVec3(shape.mOffset.x, shape.mOffset.y, shape.mOffset.z);
    const JPH::Quat rotation = JPH::Quat::sEulerAngles(JPH::Vec3(shape.mEulerAngles.x, shape.mEulerAngles.y, shape.mEulerAngles.z));
    const JPH::Vec3 scale = JPH::Vec3(1.0f, 1.0f, 1.0f);

    // Retain this until end frame
    std::unique_ptr<JPH::Shape> joltShape;

    const JPH::Color joltColor(ui8(color.r * 255.f), ui8(color.g * 255.f), ui8(color.b * 255.f), 255);

    switch (shape.mShape) {
        case CollisionShapes::Capsule: {
            joltShape = std::make_unique<JPH::CapsuleShape>(shape.mHalfDims.y, shape.mHalfDims.x);
            joltShape->Draw(sPhysicsDebugRenderer.get(), JPH::DMat44::sRotationTranslation(rotation, shapePos), scale, joltColor, false, sWireframe);
            break;
        }
        case CollisionShapes::Cylinder: {
            joltShape = std::make_unique<JPH::CylinderShape>(shape.mHalfDims.y, shape.mHalfDims.x);
            joltShape->Draw(sPhysicsDebugRenderer.get(), JPH::DMat44::sRotationTranslation(rotation, shapePos), scale, joltColor, false, sWireframe);
            break;
        }
        case CollisionShapes::Box: {
            joltShape = std::make_unique<JPH::BoxShape>(JPH::Vec3(shape.mHalfDims.x, shape.mHalfDims.y, shape.mHalfDims.z));
            joltShape->Draw(sPhysicsDebugRenderer.get(), JPH::DMat44::sRotationTranslation(rotation, shapePos), scale, joltColor, false, sWireframe);
            break;
        }
        case CollisionShapes::Sphere: {
            joltShape = std::make_unique<JPH::SphereShape>(shape.mHalfDims.x);
            joltShape->Draw(sPhysicsDebugRenderer.get(), JPH::DMat44::sRotationTranslation(rotation, shapePos), scale, joltColor, false, sWireframe);
            break;
        }
        default:
            break;
    }

    sPhysicsDebugRenderer->EndFrame();

    if (sDisableDepth) {
        vg::DepthState::restorePrevious();
    }

#else
    __debugbreak(); // Should not be possible, we require the shapes generated from JPH
#endif
}
