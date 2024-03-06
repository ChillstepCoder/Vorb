#pragma once

#include "yojimbo/yojimbo.h"

#define NET_SERIALIZE_DEF(ClassName, ...) \
template <typename Stream> \
bool ClassName::netSerialize(Stream& stream) { \
    __VA_ARGS__ \
    return true; \
} \
template bool ClassName::netSerialize<yojimbo::ReadStream>(yojimbo::ReadStream& stream); \
template bool ClassName::netSerialize<yojimbo::WriteStream>(yojimbo::WriteStream& stream); \
template bool ClassName::netSerialize<yojimbo::MeasureStream>(yojimbo::MeasureStream& stream); 