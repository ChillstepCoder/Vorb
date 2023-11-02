#pragma once

#include <boost/container/flat_map.hpp>

template<typename T>
inline const boost::container::flat_map<T, std::string_view>& getGlobalEnumNameMap() {
    panic("Enum name map not implemented for this type");
}