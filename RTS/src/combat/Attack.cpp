#include "stdafx.h"
#include "Attack.h"

KEG_ENUM_DEF(AttackShape, AttackShape, kt) {
    kt.addValue("cone", AttackShape::CONE);
    kt.addValue("sphere", AttackShape::SPHERE);
}
static_assert(e_cast(AttackShape::COUNT) == 2, "Update def");