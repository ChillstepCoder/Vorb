#include "stdafx.h"
#include "WorldContextObject.h"

#include "world/World.h"

WorldNetMode WorldContextObject::getNetMode() const {
    return mWorld.getNetMode();
}

bool WorldContextObject::isEditor() const {
    return mWorld.isEditorWorld();
}
