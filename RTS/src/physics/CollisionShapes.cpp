#include "stdafx.h"
#include "CollisionShapes.h"


KEG_ENUM_DEF(CollisionShapes, CollisionShapes, kt) {
    kt.addValue("none", CollisionShapes::COUNT);
    kt.addValue("capsule", CollisionShapes::CAPSULE);
}
static_assert(e_cast(CollisionShapes::COUNT) == 1, "Update def");