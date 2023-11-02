#pragma once

#include <ryml.hpp>
#include <c4/format.hpp>
#include <c4/std/string_view.hpp>
#include <ryml_std.hpp>

//  TODO #If WITH_EDITOR
// TODO: Try to get this UI shit out of here? Its included in stdafx
#include <imgui.h>
#include "ui/imgui_controls/EnumCombo.h"

#include "util/GlobalEnumNameMap.h"

#include "resources/asset/SoftAssetReference.h"

namespace c4 {
    namespace yml {
        namespace impl {}
    }
}

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

// Usage: ns::MyType, MyType, pair{EnumName1, "name1"sv}, pair{EnumName2, "name2"sv}, ...
#define SERIALIZABLE_ENUM(Type, TypeNoNamespace, ...) \
template<> \
inline const boost::container::flat_map<Type, std::string_view>& getGlobalEnumNameMap() { \
    using namespace std; \
    static const boost::container::flat_map<Type, std::string_view> sNameLookup = { __VA_ARGS__ }; \
    return sNameLookup; \
} \
namespace c4::yml { \
   namespace impl { \
        using namespace std; \
        inline void write(c4::yml::NodeRef* n, Type const& v) { \
            const c4::csubstr substr = c4::to_csubstr(getGlobalEnumNameMap<Type>().at(v)); \
            *n << substr;\
        } \
        inline bool read(c4::yml::ConstNodeRef const& n, Type* v) { \
            c4::csubstr s; \
            n >> s; \
            const auto& m = getGlobalEnumNameMap<Type>(); \
            std::string_view searchStr(s.data(), s.size()); \
            auto findResult = std::find_if(std::begin(m), std::end(m), [&](const std::pair<Type, std::string_view>& pair) { \
                return pair.second == searchStr; \
            }); \
            if (findResult != std::end(m)) { \
                *v = findResult->first; \
            } \
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
   getGlobalEnumNameMap<TypeNoNamespace>().at(val)

#define ENUM_CSTR(TypeNoNamespace, val) \
   getGlobalEnumNameMap<TypeNoNamespace>().at(val).data()

#define ENUM_NAME_MAP(TypeNoNamespace) \
   getGlobalEnumNameMap<TypeNoNamespace>()


// Usage: MyType, pair{EnumName1, "name1"sv}, pair{EnumName2, "name2"sv}, ...
#define SERIALIZABLE_ENUM_SAME_NAME(Type, ...) SERIALIZABLE_ENUM(Type, Type, __VA_ARGS__)

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
    bool updateAndRenderImgui() { return false; }
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
            changed |= ImguiUtil::EnumCombo<First>(label.data(), value);
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
        else if constexpr (std::is_same_v<First, f32v2>) {
            changed |= ImGui::InputFloat2(label.data(), &value.x);
        }
        else if constexpr (std::is_same_v<First, f32v3>) {
            changed |= ImGui::InputFloat3(label.data(), &value.x);
        }
        else if constexpr (std::is_same_v<First, f32v4>) {
            changed |= ImGui::InputFloat4(label.data(), &value.x);
        }
        else if constexpr (std::is_same_v<First, i32v2>) {
            changed |= ImGui::InputInt2(label.data(), &value.x);
        }
        else if constexpr (std::is_same_v<First, i32v3>) {
            changed |= ImGui::InputInt3(label.data(), &value.x);
        }
        else if constexpr (std::is_same_v<First, i32v4>) {
            changed |= ImGui::InputInt4(label.data(), &value.x);
        }
        else if constexpr (std::is_same_v<First, ui32v2>) {
            changed |= ImGui::InputScalarN(label.data(), ImGuiDataType_U32, &value.x, 2);
        }
        else if constexpr (std::is_same_v<First, ui32v3>) {
            changed |= ImGui::InputScalarN(label.data(), ImGuiDataType_U32, &value.x, 3);
        }
        else if constexpr (std::is_same_v<First, ui32v4>) {
            changed |= ImGui::InputScalarN(label.data(), ImGuiDataType_U32, &value.x, 4);
        }
        else if constexpr (std::is_same_v<First, SoftAssetReference>) {
            changed |= ImguiUtil::updateAndRenderSoftAssetReference(first);
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
