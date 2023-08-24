#pragma once

#include <ryml.hpp>
#include <c4/format.hpp>
#include <ryml_std.hpp> // optional header, provided for std:: interop

namespace YmlSerializer {
    template<typename T>
    void SerializeYmlFields(ryml::Tree& s) {}

    template<typename T>
    void DeserializeYmlFields(ryml::Tree& s) {}

    // Recursive serialize/deserialize field for variadic templates
    template<typename T, typename First, typename... Rest>
    void SerializeYmlFields(ryml::Tree& s, const First& first, const char* firstName, const Rest&... rest) {
        s[firstName] << first;
        SerializeYmlFields<T>(s, rest...);
    }

    template<typename T, typename First, typename... Rest>
    void DeserializeYmlFields(ryml::Tree& s, First& first, const char* firstName, Rest&... rest) {
        s[firstName] >> first;
        DeserializeYmlFields<T>(s, rest...);
    }

    // The serialize and deserialize functions
    template<typename T>
    void SerializeYml(ryml::Tree& s, const T& o) { assert(false && "SERIALIZABLE not provided for this type"); }

    template<typename T, typename... Fields>
    void DeserializeYml(ryml::Tree& s, T& o) { assert(false && "SERIALIZABLE not provided for this type"); }
}

// Usage: SERIALIZABLE_SIMPLE(Type, o.Value, "value_name", ...)
#define SERIALIZABLE_SIMPLE(Type, ...) \
    template <> \
    void YmlSerializer::SerializeYml(ryml::Tree& s, const Type& o) { \
        SerializeYmlFields<Type>(s, __VA_ARGS__); \
    } \
    template <> \
    void YmlSerializer::DeserializeYml(ryml::Tree& s, Type& o) { \
        DeserializeYmlFields<Type>(s, __VA_ARGS__); \
    }

// Custom types
namespace c4 {
    namespace yml {
        // All glm vector types
        template <int N, typename T>
        void write(c4::yml::NodeRef* n, glm::vec<N, T, glm::defaultp> const& v)
        {
            *n |= c4::yml::SEQ;
            for (int i = 0; i < N; ++i)
                n->append_child() << v[i];
        }
        template <int N, typename T>
        bool read(c4::yml::ConstNodeRef const& n, glm::vec<N, T, glm::defaultp>* v)
        {
            if (n.num_children() != N) return false;
            int i = 0;
            for (auto const ch : n)
                ch >> (*v)[i++];
            return true;
        }
    }
}

// TODO: UTILS
// https://xuhuisun.com/post/c++-weekly-2-constexpr-map/
template <typename Key, typename Value, std::size_t Size>
struct ConstexprMap {
    std::array<std::pair<Key, Value>, Size> data;

    [[nodiscard]] constexpr Value at(const Key& key) const {
        const auto itr =
            std::find_if(begin(data), end(data),
                [&key](const auto& v) { return v.first == key; });
        if (itr != end(data)) {
            return itr->second;
        }
        else {
            throw std::range_error("Not Found");
        }
    }
};

// Chatgpt deduce size:
template <typename Key, typename Value, std::size_t Size>
struct Map2 {
    std::array<std::pair<Key, Value>, Size> data;

    [[nodiscard]] constexpr Value at(const Key& key) const {
        const auto itr = std::find_if(begin(data), end(data),
            [&key](const auto& v) { return v.first == key; });

        if (itr != end(data)) {
            return itr->second;
        }
        else {
            throw std::range_error("Not Found");
        }
    }
};
//The Map class remains largely unchanged.The change is in the addition of a deduction guide after the Map class definition.This tells the compiler how to deduce
// the template arguments for a class template based on the constructor arguments.In this case, it says that when you create a Map from an std::array with a known size Size, 
// the compiler should use that size as the Size template parameter for the Map class.
//Here's how you can use this new version of the Map class:
template <typename Key, typename Value, std::size_t Size>
Map2(const std::array<std::pair<Key, Value>, Size>&) -> Map2<Key, Value, Size>;

int lookup_value(const std::string_view sv) {
    using namespace std::literals::string_view_literals;

    static constexpr auto map = Map{
        {{"black"sv, 7},
         {"blue"sv, 3},
         {"cyan"sv, 5},
         {"green"sv, 2},
         {"magenta"sv, 6},
         {"red"sv, 1},
         {"white"sv, 8},
         {"yellow"sv, 4}}
    };

    return map.at(sv);
}


x;
// Usage: 
#define SERIALIZABLE_ENUM(Type, ...) \
namespace c4 { \
namespace yml { \
ConstexprMap \
inline static std::map< const char* const s##Type##ToStr[] = { \
    __VA_ARGS__ \
} \
 void write(c4::yml::NodeRef* n, glm::vec<N, T, glm::defaultp> const& v) \
{ \
 \
} \
bool read(c4::yml::ConstNodeRef const& n, glm::vec<N, T, glm::defaultp>* v) \
{ \
} \
 \
} \
} 