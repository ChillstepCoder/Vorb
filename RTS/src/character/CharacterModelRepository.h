#pragma once

DECL_VG(class TextureCache);

struct CharacterModelComponent;
class SpriteRepository;

typedef std::vector<nString> CharacterModelPartSet;

class CharacterModelRepository
{
public:
    CharacterModelRepository(SpriteRepository& spriteRepository);

    void gatherCharacterModelParts();

    void initRandomCharacterModelAsRandomGender(CharacterModelComponent& cmp);
    void initRandomCharacterModelAsFemale(CharacterModelComponent& cmp);
    void initRandomCharacterModelAsMale(CharacterModelComponent& cmp);

private:
    void gatherPartsInDirectory(vio::Path directoryPath, CharacterModelPartSet& outputSet);
    const nString& getRandomPart(CharacterModelPartSet& genderSet, CharacterModelPartSet& androgynousSet);

    CharacterModelPartSet maleHairParts;
    CharacterModelPartSet maleBodyParts;
    CharacterModelPartSet maleFaceParts;
    CharacterModelPartSet femaleHairParts;
    CharacterModelPartSet femaleBodyParts;
    CharacterModelPartSet femaleFaceParts;
    CharacterModelPartSet androgynousHairParts;
    CharacterModelPartSet androgynousBodyParts;
    CharacterModelPartSet androgynousFaceParts;

    SpriteRepository& mSpriteRepository;

};

