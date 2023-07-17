#pragma once

enum class AttackShape {
    CONE,
    SPHERE,
    COUNT
};
KEG_ENUM_DECL(AttackShape);

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
    ui16v2 damageRange;
    BitFlags<AttackFlags> flags;
};
