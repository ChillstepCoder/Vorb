#include "stdafx.h"
#include "Attack.h"

SERIALIZABLE_ENUM_SAME_NAME(AttackShape,
    pair{ AttackShape::CONE, "cone"sv },
    pair{ AttackShape::SPHERE, "sphere"sv }
);
static_assert(e_count(AttackShape) == 2, "Update def");


void AttackData::initAsCone(f32 radius, f32 arcAngleRad, f32 forwardOffset, f32 height, ui16v2 damageRange) {
    varAttackShape = AttackShapeCone{ .radius = radius, .arcAngleRad = arcAngleRad, .forwardOffset = forwardOffset, .height = height };
    this->damageRange = damageRange;
    shapeType = AttackShape::CONE;
}
