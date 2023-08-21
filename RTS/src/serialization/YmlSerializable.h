#pragma once

#define YML_SAVE_LAMBDA(obj) \
  [inobj = &obj](keg::YAMLWriter& writer) { inobj->saveYmlData(writer); }

// Simple interface to turn anything into a yml node
class YmlSerializable {
public:
    virtual ~YmlSerializable() = default;

    // Pure virtual interface
    virtual constexpr const char* const getYmlName() const = 0;
    virtual bool loadFromYml(keg::ReadContext& context, keg::Node node) const = 0;
    // End pure virtual interface

    void saveYml(keg::YAMLWriter& writer) const {
        if (mSerializeNameOnly) {
            writer.operator<<((char*)getYmlName());
        }
        else {
            beginMap(writer);
            pushKeyValue(writer, getYmlName());
            saveYmlData(writer);
            endMap(writer);
        }
    }

    static void beginMap(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::BEGIN_MAP);
    }
    static void endMap(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::END_MAP);
    }
    static void beginSequence(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::BEGIN_SEQUENCE);
    }
    static void endSequence(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::END_SEQUENCE);
    }
    static void pushValue(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::VALUE);
    }
    static void pushKeyValue(keg::YAMLWriter& writer, const char* key){
        writer.push(keg::WriterParam::KEY);
        writer << const_cast<char*>(key);
        writer.push(keg::WriterParam::VALUE);
    }
    static void saveNested(keg::YAMLWriter& writer, const char* key, const YmlSerializable& nested) {
        pushKeyValue(writer, key);
        nested.saveYml(writer);
    }
    static void saveNested(keg::YAMLWriter& writer, const char* key, std::function<void(keg::YAMLWriter&)> nestedFunc) {
        pushKeyValue(writer, key);
        nestedFunc(writer);
    }
    template<typename T>
    static void saveKeyValue(keg::YAMLWriter& writer, const char* key, const T& value) {
        pushKeyValue(writer, key);
        writer.operator<<(value);
    }
    template<typename T>
    static void saveValue(keg::YAMLWriter& writer, const T& value) {
        writer.push(keg::WriterParam::VALUE);
        writer.operator<<(value);
    }

protected:

    // Pure virtual interface
    virtual void saveYmlData(keg::YAMLWriter& writer) const = 0;
    // End pure virtual interface

    bool mSerializeNameOnly = false;
};