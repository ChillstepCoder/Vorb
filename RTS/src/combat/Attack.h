#pragma once

enum class AttackShape {
    CONE,
    SPHERE,
    COUNT
};
SERIALIZABLE_ENUM_DECL(AttackShape);

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

    void initAsCone(f32 radius, f32 arcAngleRad, f32 forwardOffset, f32 height, ui16v2 damageRange);

    std::variant<AttackShapeCone> varAttackShape;
    AttackShape shapeType;
    f32 swingHeight = 1.3f;
    f32v3 swingDir = f32v3(0.0f); // Direction of swinging weapon
    ui16v2 damageRange;
    BitFlags<AttackFlags> flags;
};
