#pragma once


// TODO: use boost::locale and also allow specifying a context for the translation
using LocText = std::string;


//class LocText {
//    friend class LocTextManager;
//public:
//    LocText(std::string_view text);
//
//    LocTextID getId() { return mId; }
//
//private:
//    LocTextID mId;
//};
////SERIALIZABLE_SIMPLE(LocText,
////    make_field(o.mId, "id"sv)
////);
//
//
//class LocTextManager {
//    friend class LocText;
//public:
//    ~LocTextManager();
//
//    LocTextManager& getInstance();
//
//private:
//    LocTextManager() = default;
//
//    // TODO: LANGUAGE ENUM
//    std::string_view getEnglishText(LocTextID id);
//
//    void registerEnglishText(LocTextID id, std::string_view text);
//    void onNewEnglishTextRegistered(LocTextID id, std::string_view text);
//
//    // TODO: Handle other languages
//    // TODO: wchar?
//    std::shared_mutex mMutex;
//    UnorderedFlatMap<LocTextID, const char*> mEnglishStrings;
//
//    // TODO: Dump to file
//};