#pragma once

enum class AttackShape {
    CONE,
    SPHERE,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(AttackShape,
    pair{ AttackShape::CONE, "cone"sv},
    pair{ AttackShape::SPHERE, "sphere"sv}
);
static_assert(e_count(AttackShape) == 2, "Update def");

struct AttackShapeCone {
    f32 radius;
    f32 arcAngleRad;
    f32 forwardOffset;
    f32 height;
};

enum class AttackFlags : ui8 {
    //CanHitTree
};

struct AttackData {

    void initAsCone(f32 radius, f32 arcAngleRad, f32 forwardOffset, f32 height, ui16v2 damageRange) {
        varAttackShape = AttackShapeCone{ .radius = radius, .arcAngleRad = arcAngleRad, .forwardOffset = forwardOffset, .height = height };
        this->damageRange = damageRange;
        shapeType = AttackShape::CONE;
    }

    std::variant<AttackShapeCone> varAttackShape;
    AttackShape shapeType;
    f32 swingHeight = 0.5f;
    f32v3 swingDir = f32v3(0.0f); // Direction of swinging weapon
    ui16v2 damageRange;
    BitFlags<AttackFlags> flags;
};
