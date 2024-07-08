#pragma once

#include "definitions/ModelDef.h"

// TODO: MOVE
// Static class for collision editing utility

class CollisionEditor {
public:
    static bool updateAndRenderImguiControlsForShapeVector(std::vector<ModelColliderShape>& shapes);
    static bool updateAndRenderImguiControlsForShape(ModelColliderShape& shape);
    static void renderShapesInEditor(const std::vector<ModelColliderShape>& shapes);
    static void renderShapeInEditor(const ModelColliderShape& shape);
};