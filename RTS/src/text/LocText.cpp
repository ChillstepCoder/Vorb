#include "stdafx.h"
#include "LocText.h"

//
//LocText::LocText(std::string_view text) {
//    mId = boost::hash_range(text.begin(), text.end());
//}
//
//LocTextManager::~LocTextManager() {
//    for (auto& [id, text] : mEnglishStrings) {
//        delete[] text;
//    }
//}
//
//LocTextManager& LocTextManager::getInstance() {
//    static LocTextManager sInstance;
//    return sInstance;
//}
//
//std::string_view LocTextManager::getEnglishText(LocTextID id) {
//    std::shared_lock lock(mMutex); // SHARED LOCK
//    auto&& it = mEnglishStrings.find(id);
//    if (it != mEnglishStrings.end()) {
//        return it->second;
//    }
//    else {
//        return "MISSING TEXT";
//    }
//}
//
//void LocTextManager::registerEnglishText(LocTextID id, std::string_view text) {
//    mMutex.lock_shared(); // SHARED LOCK
//    auto&& it = mEnglishStrings.find(id);
//    if (it != mEnglishStrings.end()) {
//        // Our string is already registered from file
//#ifdef DEBUG
//        if (strcmp(it->second, text.data()) != 0) {
//            panic("LocText hash collision detected for ID {} with text '{}' and '{}'", id, text, it->second);
//        }
//#endif
//        mMutex.unlock_shared(); // SHARED UNLOCK
//        return;
//    }
//    else {
//        mMutex.unlock_shared(); // SHARED UNLOCK
//        char* newText = new char[text.size() + 1];
//        memcpy(newText, text.data(), text.size() + 1);
//        {
//            std::lock_guard lock(mMutex); // EXCLUSIVE LOCK
//            mEnglishStrings.emplace(id, newText);
//            onNewEnglishTextRegistered(id, text);
//        }
//    }
//}
//
//void LocTextManager::onNewEnglishTextRegistered(LocTextID id, std::string_view text)
//{
//    // TODO: UPDATE FILE
//}
