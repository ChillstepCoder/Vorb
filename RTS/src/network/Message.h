#pragma once

#include <yojimbo/yojimbo.h>

enum class MessageTypes {
    TEST,
    COUNT
};

struct TestMessage : public yojimbo::Message {

    template <typename Stream> bool Serialize(Stream& stream) {
        // serialize_bits
        serialize_float(stream, mData);

        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    f32 mData = 5.3649f;
};