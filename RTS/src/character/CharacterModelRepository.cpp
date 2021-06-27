#include "stdafx.h"
#include "CharacterModelRepository.h"

#include "rendering/CharacterModel.h"

#include "Random.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/IO.h>

CharacterModelRepository::CharacterModelRepository(vg::TextureCache& textureCache) :
    mTextureCache(textureCache) {

}

void CharacterModelRepository::gatherCharacterModelParts() {
    gatherPartsInDirectory("data/textures/character/body/androgynous", androgynousBodyParts);
    gatherPartsInDirectory("data/textures/character/body/male", maleBodyParts);
    gatherPartsInDirectory("data/textures/character/body/female", femaleBodyParts);
    gatherPartsInDirectory("data/textures/character/face/androgynous", androgynousFaceParts);
    gatherPartsInDirectory("data/textures/character/face/male", maleFaceParts);
    gatherPartsInDirectory("data/textures/character/face/female", femaleFaceParts);
    gatherPartsInDirectory("data/textures/character/hair/androgynous", androgynousHairParts);
    gatherPartsInDirectory("data/textures/character/hair/male", maleHairParts);
    gatherPartsInDirectory("data/textures/character/hair/female", femaleHairParts);
}

void CharacterModelRepository::initRandomCharacterModelAsRandomGender(CharacterModelComponent& cmp) {
    PreciseTimer timer;
    if (Random::getCachedRandom() % 2 == 0) {
        initRandomCharacterModelAsFemale(cmp);
    }
    else {
        initRandomCharacterModelAsMale(cmp);
    }
    std::cout << " character model initialized in " << timer.stop() << " ms\n";
}

void CharacterModelRepository::initRandomCharacterModelAsFemale(CharacterModelComponent& cmp) {
    cmp.mModel.load(
        mTextureCache,
        getRandomPart(femaleFaceParts, androgynousFaceParts), 
        getRandomPart(femaleBodyParts, androgynousBodyParts),
        getRandomPart(femaleHairParts, androgynousHairParts)
    );
}

void CharacterModelRepository::initRandomCharacterModelAsMale(CharacterModelComponent& cmp) {
    cmp.mModel.load(
        mTextureCache,
        getRandomPart(maleFaceParts, androgynousFaceParts),
        getRandomPart(maleBodyParts, androgynousBodyParts),
        getRandomPart(maleHairParts, androgynousHairParts)
    );
}

void CharacterModelRepository::gatherPartsInDirectory(vio::Path directoryPath, CharacterModelPartSet& outputSet) {
    if (!directoryPath.isValid()) {
        return;
    }

    std::set<nString> alreadyFound;
    
    vio::Directory directory;
    if (!directoryPath.asDirectory(&directory)) {
        // TODO: Better error messaging
        assert(false);
    }

    vio::DirectoryEntries entries;
    if (!directory.appendEntries(entries)) {
        // Empty directory
        return;
    }

    for (auto&& entry : entries) {
        if (entry.isDirectory()) {
            gatherPartsInDirectory(directoryPath, outputSet);
        }
        else {
            // Trim the end, for example _side.png
            // The character model will automatically grab all files
            // when it loads.
            nString fileStr = entry.getString();
            for (int i = fileStr.size() - 1; i >= 0; --i) {
                if (fileStr[i] == '_') {
                    fileStr.resize(i);
                    break;
                }
            }
            assert(fileStr.size());
            if (alreadyFound.find(fileStr) == alreadyFound.end()) {
                alreadyFound.insert(fileStr);
                outputSet.emplace_back(fileStr);
            }
        }
    }
}

const nString& CharacterModelRepository::getRandomPart(CharacterModelPartSet& genderSet, CharacterModelPartSet& androgynousSet) {
    const size_t totalCount = genderSet.size() + androgynousSet.size();
    assert(totalCount);
    const ui32 index = Random::getCachedRandom() % totalCount;
    if (index >= genderSet.size()) {
        return (androgynousSet[index - genderSet.size()]);
    }
    else {
        return genderSet[index];
    }
}
