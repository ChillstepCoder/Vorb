#pragma once

#include <ryml.hpp>
#include <c4/format.hpp>
#include <c4/std/string_view.hpp>
#include <ryml_std.hpp>

#include "util/ConstexprMap.h"

namespace YmlSerializer {
    template<typename T>
    void serializeYmlFields(ryml::NodeRef& s) {}

    template<typename T>
    void deserializeYmlFields(const ryml::ConstNodeRef& s) {}

    // Recursive serialize/deserialize field for variadic templates
    template<typename T, typename First, typename... Rest>
    void serializeYmlFields(ryml::NodeRef& s, const First& first, std::string_view firstName, const Rest&... rest) {
        // TODO: Dont serialize if default?
        s[ryml::to_csubstr(firstName)] << first;
        serializeYmlFields<T>(s, rest...);
    }

    template<typename T, typename First, typename... Rest>
    void deserializeYmlFields(const ryml::ConstNodeRef& s, First& first, std::string_view firstName, Rest... rest) {
        c4::csubstr nameSubstr = ryml::to_csubstr(firstName);
        LOG_CRITICAL(" {} ", (int)s.type());
        if (s.has_child(nameSubstr)) {
            s[nameSubstr] >> first;
        }
        deserializeYmlFields<T>(s, rest...);
    }

    // The serialize and deserialize functions
    template<typename T>
    void serializeYml(ryml::NodeRef& s, const T& o) { assert(false && "SERIALIZABLE not provided for this type"); }

    template<typename T, typename... Fields>
    void deserializeYml(const ryml::ConstNodeRef& s, T& o) { assert(false && "SERIALIZABLE not provided for this type"); }

    template<typename T>
    void deserializeYml(const nString& ymlFileData, T& o) {
        ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(ymlFileData));
        assert((int)tree.crootref().type() > 1);
        deserializeYml(tree.crootref(), o);
    }

    ryml::Tree parseFileData(const nString& ymlFileData) {
        return ryml::parse_in_arena(ryml::to_csubstr(ymlFileData));
    }
}

// Usage: SERIALIZABLE_SIMPLE(Type, o.Value, "value_name"sv, ...)
#define SERIALIZABLE_SIMPLE(Type, ...) \
    template <> \
    void YmlSerializer::serializeYml(ryml::NodeRef& s, const Type& o) { \
        serializeYmlFields<Type>(s, __VA_ARGS__); \
    } \
    template <> \
    void YmlSerializer::deserializeYml(const ryml::ConstNodeRef& s, Type& o) { \
        deserializeYmlFields<Type>(s, __VA_ARGS__); \
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