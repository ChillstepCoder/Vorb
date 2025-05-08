#pragma once

template<typename T>
inline const boost::container::flat_map<T, std::string_view>& getGlobalEnumNameMap() {
    panic("Enum name map not implemented for this type");
}

// Helper that will try to match this string with this enum type, and return the result in outE
template <typename E, typename OutType>
inline bool tryReadEnum(const std::string_view str, OutType& outE) {
    const auto& map = getGlobalEnumNameMap<E>();
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (str.compare(it->second) == 0) {
            outE = it->first;
            return true;
        }
    }
    return false;
}

template<typename T>
constexpr bool is_enum_class_v = std::is_enum_v<T> && !std::is_convertible_v<T, int>;

// Helper that will compare a string to all possible string values of a variant of enums, and return the correct
// enum in outE or return false if it cant find it
template<typename Tuple, typename OutType, std::size_t Index = 0>
inline bool tryReadAnyEnumFromTupleIntoVariant(const std::string_view str, OutType& outVariant) {
    // If null terminated, remove the null terminator
    std::string_view strNoNull = str;
    if (strNoNull.back() == '\0') {
        strNoNull.remove_suffix(1);
    }

    // Don't process invalid enums
    if (strNoNull.compare("INVALID"sv) == 0) {
        return false;
    }
    if constexpr (Index < std::tuple_size<Tuple>::value) {
        using EnumType = std::tuple_element_t<Index, Tuple>;
        if constexpr (is_enum_class_v<EnumType>) {
            if (tryReadEnum<EnumType, OutType>(strNoNull, outVariant)) {
                return true;
            }
            else {
                return tryReadAnyEnumFromTupleIntoVariant<Tuple, OutType, Index + 1>(strNoNull, outVariant);
            }
        }
        else {
            return tryReadAnyEnumFromTupleIntoVariant<Tuple, OutType, Index + 1>(strNoNull, outVariant);
        }
    }
    else {
        return false;
    }
}