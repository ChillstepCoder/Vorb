#pragma once

#include "rendering/GlobalUboData.h"

struct GlobalRenderData {
    GlobalUboData globalUboData;
    f32 cameraZAngle;
    f32m4 skyRotMatrix;
};

