#pragma once

enum class AttributeType {
    Health,
    Stamina,
    Blood,
    MoveSpeed,
    COUNT
};

constexpr f32 DEFAULT_HEALTH = 100.0f;
constexpr f32 DEFAULT_STAMINA = 100.0f;
constexpr f32 DEFAULT_BLOOD = 100.0f;
constexpr f32 DEFAULT_MOVE_SPEED = 2.0f; // TODO: Figure out proper values

struct DualAttributesComponent {

    void init(f32 health, f32 stamina, f32 blood, f32 moveSpeed) {
        mAttributeRanges[e_cast(AttributeType::Health)] = f32v2(health, health);
        mAttributeRanges[e_cast(AttributeType::Stamina)] = f32v2(stamina, stamina);
        mAttributeRanges[e_cast(AttributeType::Blood)] = f32v2(blood, blood);
        mAttributeRanges[e_cast(AttributeType::MoveSpeed)] = f32v2(moveSpeed, moveSpeed);
    };

    // TODO: Determine how downed state works
    bool isDead() const { return getCurrentAttribute(AttributeType::Health) <= -1.0f; }
    bool isDowned() const { return getCurrentAttribute(AttributeType::Health) <= 0.0f; }

    f32 getCurrentAttribute(AttributeType attribute) const { return mAttributeRanges[e_cast(attribute)].x; }
    f32 getMaxAttribute(AttributeType attribute) const { return mAttributeRanges[e_cast(attribute)].y; }

    void setCurrentAttribute(AttributeType attribute, f32 val) { mAttributeRanges[e_cast(attribute)].x = val; }
    void setMaxAttribute(AttributeType attribute, f32 max) { mAttributeRanges[e_cast(attribute)].y = max; }

    f32v2 mAttributeRanges[e_cast(AttributeType::COUNT)]; //[Curr, Max]
};
static_assert(e_cast(AttributeType::COUNT) == 4);

