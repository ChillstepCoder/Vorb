#include "stdafx.h"
#include "CollisionShapes.h"


KEG_ENUM_DEF(CollisionShapes, CollisionShapes, kt) {
    kt.addValue("none", CollisionShapes::COUNT);
    kt.addValue("capsule", CollisionShapes::CAPSULE);
    kt.addValue("cylinder", CollisionShapes::CYLINDER);
    kt.addValue("box", CollisionShapes::BOX);
    kt.addValue("sphere", CollisionShapes::SPHERE);
}
static_assert(e_cast(CollisionShapes::COUNT) == 4, "Update def");