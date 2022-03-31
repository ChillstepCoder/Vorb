#pragma once

enum class AttributeType {
    HEALTH,
    STAMINA,
    BLOOD,
    COUNT
};

struct AttributesComponent {

    // TODO: Determine how downed state works
    bool isDead() const { return getCurrentAttribute(AttributeType::HEALTH) <= -1.0f; }
    bool isDowned() const { return getCurrentAttribute(AttributeType::HEALTH) <= 0.0f; }

    f32 getCurrentAttribute(AttributeType attribute) const { return mAttributeRanges[e_cast(attribute)].x; }
    f32 getMaxAttribute(AttributeType attribute) const { return mAttributeRanges[e_cast(attribute)].y; }

    void setCurrentAttribute(AttributeType attribute, f32 val) { mAttributeRanges[e_cast(attribute)].x = val; }
    void setMaxAttribute(AttributeType attribute, f32 max) { mAttributeRanges[e_cast(attribute)].y = max; }

    f32v2 mAttributeRanges[e_cast(AttributeType::COUNT)]; //[Curr, Max]
};
static_assert(e_cast(AttributeType::COUNT) == 3);

