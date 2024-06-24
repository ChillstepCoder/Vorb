#pragma once

#define YML_SAVE_LAMBDA(obj) \
  [inobj = &obj](keg::YAMLWriter& writer) { inobj->saveYmlData(writer); }

#include <ryml.hpp>
#include "serialization/YmlSerializer.h"

// Simple interface to turn anything into a yml node
class YmlSerializable {
public:
    virtual ~YmlSerializable() = default;

    // Pure virtual interface
    virtual constexpr const char* const getYmlName() const = 0;
    virtual bool loadFromYml(ryml::ConstNodeRef node) = 0;
    // End pure virtual interface

    virtual void saveYml(ryml::NodeRef parentNode) const {
        ryml::NodeRef thisNode = parentNode[getYmlName()];
        thisNode |= ryml::MAP;
        saveYmlData(thisNode);
    }

    static void saveWithLambda(ryml::NodeRef parentNode, std::string_view key, std::function<void(ryml::NodeRef)> saveFunc) {
        ryml::NodeRef newNode = parentNode[c4::to_csubstr(key)];
        saveFunc(newNode);
    }

protected:
    virtual void saveYmlData(ryml::NodeRef node) const {};
};

template<typename T>
struct GlobalYmlMap {
    std::mutex mMutex;
    UnorderedFlatMap<nString, std::unique_ptr<T>> mMap;
};

namespace yml {
    template<typename T>
    inline GlobalYmlMap<T>& objectMap() {
        static GlobalYmlMap<T> globalMap;
        return globalMap;
    }

    template<typename T>
    inline std::unique_ptr<T> cloneYmlObject(c4::csubstr name) {
        std::string_view sv(name.data(), name.size());
        nString key(sv);
        GlobalYmlMap<T>& globalMap = objectMap<T>();
        std::lock_guard lock(globalMap.mMutex);
        return globalMap.mMap.at(key)->clone();
    }

    template<typename T>
    inline const UnorderedFlatMap<nString, std::unique_ptr<T>>& getAllObjects() {
        return objectMap<T>().mMap;
    }

    template<typename T>
    inline const bool tryReadValue(ryml::ConstNodeRef node, std::string_view key, T& value) {
        c4::csubstr ckey = c4::to_csubstr(key);
        if (node.has_child(ckey)) {
            node[ckey].operator>>(value);
            return true;
        }
        return false;
    }
};

template<typename T, typename Base>
concept HasCloneMethod = requires(T a) {
    { a.clone() } -> std::same_as<std::unique_ptr<Base>>; // Must have clone function which returns unique_ptr
};

template<typename Base, HasCloneMethod<Base> T>
class RegisterInYmlObjectMap {
public:
    RegisterInYmlObjectMap(const std::string& key) {
        GlobalYmlMap<Base>& globalMap = yml::objectMap<Base>();
        std::lock_guard lock(globalMap.mMutex);
        globalMap.mMap[key] = std::make_unique<T>();
    }
};

// Make sure baseClass is the first class above YmlSerializable. All objects will be returned
// with that type.
#define REGISTER_YML_OBJECT(key, object, baseClass) \
    inline static RegisterInYmlObjectMap<baseClass, object> register_##object(key)