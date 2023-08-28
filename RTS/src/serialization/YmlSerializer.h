#pragma once

#include <ryml.hpp>
#include <c4/format.hpp>
#include <c4/std/string_view.hpp>
#include <ryml_std.hpp>

#include "util/ConstexprMap.h"

template<typename T>
struct FieldPair {
    T& value;
    std::string_view key;
};

template<typename T>
FieldPair<T> make_field(T& val, std::string_view k) {
    return FieldPair<T>{ val, k };
}

namespace YmlSerializer {
    template<typename T>
    void serializeYmlFields(ryml::NodeRef& s) {}

    template<typename T>
    void deserializeYmlFields(const ryml::ConstNodeRef& s) {}

    // Recursive serialize/deserialize field for variadic templates
    template<typename T, typename First, typename... Rest>
    void serializeYmlFields(ryml::NodeRef& s, const FieldPair<First> first, const Rest&... rest) {
        // TODO: Dont serialize if default?
        s[ryml::to_csubstr(first.key)] << first.value;
        serializeYmlFields<T>(s, rest...);
    }

    template<typename T, typename First, typename... Rest>
    void deserializeYmlFields(ryml::ConstNodeRef const& s, FieldPair<First> first, Rest... rest) {
        c4::csubstr nameSubstr = ryml::to_csubstr(first.key);
        if (s.has_child(nameSubstr)) {
            s[nameSubstr] >> first.value;
        }
        deserializeYmlFields<T>(s, rest...);
    }

    inline ryml::Tree parseFileData(const nString& ymlFileData) {
        return ryml::parse_in_arena(ryml::to_csubstr(ymlFileData));
    }

    template<typename T>
    void readFileData(const nString& ymlFileData, T& o) {
        ryml::Tree tree = parseFileData(ymlFileData);
        assert((int)tree.crootref().type() > 1);
        read(tree.crootref(), &o);
    }
}

#define YML_WRITE_DEF(Type) inline void write(c4::yml::NodeRef* n, Type const& o)
#define YML_READ_DEF(Type) inline bool read(c4::yml::ConstNodeRef const& n, Type* target)

// Usage: SERIALIZABLE_SIMPLE(Type, make_field(o.Value1, "value_name1"sv), make_field(o.Value2, ...)
#define SERIALIZABLE_SIMPLE(Type, ...) \
    YML_WRITE_DEF(Type) { \
        YmlSerializer::serializeYmlFields<Type>(*n, __VA_ARGS__); \
    } \
    YML_READ_DEF(Type) { \
        Type& o = *target; \
        YmlSerializer::deserializeYmlFields<Type>(n, __VA_ARGS__); \
        return true; \
    }

YML_WRITE_DEF(color4) {
    ryml::NodeRef& nr = *n;
    nr |= ryml::SEQ;
    for (int i = 0; i < 4; ++i) {
        nr << o[i];
    }
}
YML_READ_DEF(color4) {
    if (n.type() != ryml::SEQ) {
        return false;
    }
    if (n.num_children() != 4) return false;
    int i = 0;
    for (auto const ch : n)
        ch >> (*target)[i++];
    return true;
}

// Custom types
namespace c4 {
    namespace yml {

        // All glm vector types
        template <int N, typename T>
        void write(c4::yml::NodeRef* n, glm::vec<N, T, glm::defaultp> const& v)
        {
            *n |= c4::yml::SEQ;
            *n |= ryml::_WIP_STYLE_FLOW_SL;
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


// Usage: ns::MyType, MyType, pair{EnumName1, "name1"sv}, pair{EnumName2, "name2"sv}, ...
#define SERIALIZABLE_ENUM(Type, TypeNoNamespace, ...) \
namespace c4::yml { \
   namespace impl { \
        using namespace std; \
        inline constexpr auto s##TypeNoNamespace##NameLookup = ConstexprMap( \
            std::array{ \
            __VA_ARGS__ \
            } \
        ); \
        inline void write(c4::yml::NodeRef* n, Type const& v) { \
            const c4::csubstr substr = c4::to_csubstr(s##TypeNoNamespace##NameLookup[v]); \
            *n << substr;\
        } \
        inline bool read(c4::yml::ConstNodeRef const& n, Type* v) { \
            c4::csubstr s; \
            n >> s; \
            *v = s##TypeNoNamespace##NameLookup.getKeyForValue(std::string_view(s.data(), s.size())); \
            return true; \
        } \
    } \
    inline void write(c4::yml::NodeRef* n, Type const& v) { \
        impl::write(n, v); \
    } \
    inline bool read(c4::yml::ConstNodeRef const& n, Type* v) { \
        return impl::read(n, v); \
    } \
} 

#define SERIALIZABLE_ENUM_SAME_NAME(Type, ...) SERIALIZABLE_ENUM(Type, Type, __VA_ARGS__)