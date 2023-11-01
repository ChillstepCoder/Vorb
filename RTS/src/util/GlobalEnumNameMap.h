#pragma once

template<typename T>
inline const std::map<T, std::string_view>& getGlobalEnumNameMap() {
    panic("Enum name map not implemented for this type");
}