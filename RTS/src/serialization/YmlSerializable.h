#pragma once

// Simple interface to turn anything into a yml node
class YmlSerializable {
public:
    virtual ~YmlSerializable() = default;

    // Pure virtual interface
    virtual const char* const getYmlName() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterModule> loadFromYml(keg::ReadContext& context, keg::Node node) const = 0;
    // End pure virtual interface

    static void beginMap(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::BEGIN_MAP);
    }
    static void endMap(keg::YAMLWriter& writer) {
        writer.push(keg::WriterParam::END_MAP);
    }
    static void pushKeyValue(keg::YAMLWriter& writer, const char* key){
        writer.push(keg::WriterParam::KEY);
        writer << key;
        writer.push(keg::WriterParam::VALUE);
    }
    static void saveNested(keg::YAMLWriter& writer, const char* key, const YmlSerializable& nested) {
        pushKeyValue(writer, key);
        nested.saveYmlData(writer);
    }

protected:

    // Pure virtual interface
    virtual void saveYmlData(keg::YAMLWriter& writer) const = 0;
    // End pure virtual interface
};