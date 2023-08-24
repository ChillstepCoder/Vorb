#pragma once

#include <ryml.hpp>
#include <c4/format.hpp>
#include <c4/std/string_view.hpp>
#include <ryml_std.hpp> // optional header, provided for std:: interop

#include "util/ConstexprMap.h"

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

// Usage: pair{EnumName1, "name1"sv}, pair{EnumName2, "name2"sv}, ...
#define SERIALIZABLE_ENUM(Type, ...) \
namespace c4 { \
namespace yml { \
   namespace { \
   using namespace std; \
   using namespace std::literals; \
   using enum Type; \
    constexpr auto s##Type##NameLookup = ConstexprMap( \
        std::array{ \
        __VA_ARGS__ \
        } \
    ); \
    void write(c4::yml::NodeRef* n, Type const& v) \
    { \
        const c4::csubstr substr = c4::to_csubstr(s##Type##NameLookup[v]); \
        *n << substr;\
    } \
    bool read(c4::yml::ConstNodeRef const& n, Type* v) \
    { \
        c4::csubstr s; \
        n >> s; \
        *v = s##Type##NameLookup.getKeyForValue(std::string_view(s.data(), s.size())); \
        return true; \
    } \
    } \
} \
} 