#pragma once

#include <ryml.hpp>
#include <c4/format.hpp>
#include <c4/std/string_view.hpp>
#include <ryml_std.hpp>

#include "util/ConstexprMap.h"

//  TODO #If WITH_EDITOR
#include <imgui.h>

template<typename T>
struct FieldPair {
    T& value;
    std::string_view key;
};

template<typename T>
FieldPair<T> make_field(T& val, std::string_view k) {
    return FieldPair<T>{ val, k };
}

//// A helper type trait to check if a type is a specialization of std::vector
//template<typename>
//struct is_std_vector : std::false_type {};
//
//template<typename U>
//struct is_std_vector<std::vector<U>> : std::true_type {};

namespace YmlSerializer {
    template<typename T>
    void serializeYmlFields(ryml::NodeRef& s) {}
    // Recursive serialize/deserialize field for variadic templates
    template<typename T, typename First, typename... Rest>
    void serializeYmlFields(ryml::NodeRef& s, const FieldPair<First> first, const Rest&... rest) {
        // TODO: Dont serialize if default?
        s |= ryml::MAP;
        s[ryml::to_csubstr(first.key)] << first.value;

        serializeYmlFields<T>(s, rest...);
    }

    template<typename T>
    void deserializeYmlFields(const ryml::ConstNodeRef& s) {}
    template<typename T, typename First, typename... Rest>
    void deserializeYmlFields(ryml::ConstNodeRef const& s, FieldPair<First> first, Rest... rest) {
        c4::csubstr nameSubstr = ryml::to_csubstr(first.key);
        if (s.has_child(nameSubstr)) {
            s[nameSubstr] >> first.value;
        }
        deserializeYmlFields<T>(s, rest...);
    }

    template<typename T>
    bool updateAndRenderImgui() {}
    template<typename T, typename First, typename... Rest>
    bool updateAndRenderImgui(FieldPair<First> first, Rest... rest) {
        First& value = first.value;
        const std::string_view label = first.key;
        bool changed = false;
        if constexpr (std::is_floating_point_v<First>) {
            // Handle floating point types (e.g., float, double)
            changed |= ImGui::SliderFloat(label.data(), reinterpret_cast<float*>(&value), 0.0f, 100.0f);
        }
        else if constexpr (std::is_integral_v<First>) {
            // Handle integral types (e.g., int, unsigned int)
            changed |= ImGui::SliderInt(label.data(), reinterpret_cast<int*>(&value), 0, 100);
        }
        else if constexpr (std::is_enum_v<First>) {
            // Handle enum types
            // You need to provide a way to convert enum to int and back, this is just a placeholder
            int enumValue = static_cast<int>(value);
            changed |= ImGui::SliderInt(label.data(), &enumValue, 0, 100);
            if (changed) {
                value = static_cast<T>(enumValue);
            }
        }
        else if constexpr (std::is_same_v<First, std::string>) {
            // Handle std::string
            char buffer[256];
            std::strncpy(buffer, value.c_str(), sizeof(buffer));
            changed |= ImGui::InputText(label.data(), buffer, sizeof(buffer));
            if (changed) {
                value = buffer;
            }
        }
        // Add more type checks if needed
        return changed | updateAndRenderImgui<T>(rest...);
    }

    inline ryml::Tree parseFileData(const nString& ymlFileData) {
        return ryml::parse_in_arena(ryml::to_csubstr(ymlFileData));
    }
    inline ryml::Tree parseFileData(std::string_view ymlFileData) {
        return ryml::parse_in_arena(ryml::to_csubstr(ymlFileData));
    }
    inline ryml::Tree parseFileData(c4::csubstr ymlFileData) {
        return ryml::parse_in_arena(ymlFileData);
    }

    template<typename T>
    void readFileData(const nString& ymlFileData, T& o) {
        if (ymlFileData.size() <= 1) return;
        ryml::Tree tree = parseFileData(ymlFileData);
        assert((int)tree.crootref().type() > 1);
        read(tree.crootref(), &o);
    }
}

#define YML_WRITE_DEF(...) inline void write(c4::yml::NodeRef* n, __VA_ARGS__ const& o)
#define YML_READ_DEF(...) inline bool read(c4::yml::ConstNodeRef const& n, __VA_ARGS__* target)

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
#define SERIALIZABLE_IMGUI_CONTROLLED(Type, ...) \
    SERIALIZABLE_SIMPLE(Type, __VA_ARGS__) \
    inline bool updateAndRenderImguiControls(Type& o) { \
        return YmlSerializer::updateAndRenderImgui<Type>(__VA_ARGS__); \
    }

YML_WRITE_DEF(color4) {
    ryml::NodeRef& nr = *n;
    nr |= ryml::SEQ;
    nr |= ryml::_WIP_STYLE_FLOW_SL;
    for (int i = 0; i < 4; ++i) {
        nr.append_child() << o[i];
    }
}
YML_READ_DEF(color4) {
    if (n.num_children() != 4) return false;
    int i = 0;
    for (auto const ch : n)
        ch >> (*target)[i++];
    return true;
}

// Custom types
namespace c4 {
    namespace yml {
        namespace impl {}

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

        // Simple pair types
        template <typename K, typename T>
        void write(c4::yml::NodeRef* n, std::pair<K, T> const& v) {
            ryml::NodeRef& nr = *n;
            nr |= ryml::SEQ;
            nr |= ryml::_WIP_STYLE_FLOW_SL;
            nr.append_child() << v.first;
            nr.append_child() << v.second;
        }

        template <typename K, typename T>
        bool read(c4::yml::ConstNodeRef const& n, std::pair<K, T>* v) {
            if (n.num_children() != 2) return false;

            int i = 0;
            for (auto const ch : n) {
                if (i == 0) {
                    ch >> (*v).first;
                }
                else {
                    ch >> (*v).second;
                }
                ++i;
            }
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

// Usage: ENUM_STR(MyType, MyType::Val)
#define ENUM_STRV(TypeNoNamespace, val) \
   c4::yml::impl::s##TypeNoNamespace##NameLookup[val]

#define ENUM_CSTR(TypeNoNamespace, val) \
   c4::yml::impl::s##TypeNoNamespace##NameLookup[val].data()

#define ENUM_NAME_MAP(TypeNoNamespace) \
   c4::yml::impl::s##TypeNoNamespace##NameLookup


// Usage: MyType, pair{EnumName1, "name1"sv}, pair{EnumName2, "name2"sv}, ...
#define SERIALIZABLE_ENUM_SAME_NAME(Type, ...) SERIALIZABLE_ENUM(Type, Type, __VA_ARGS__)