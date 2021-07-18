#pragma once

#include "CityConst.h"

#include "city/RoomNode.h"

constexpr ui8 GRAMMAR_SEPARATOR = 254;
constexpr ui8 GRAMMAR_DELIMINATOR = 255;
constexpr ui8 MAX_GRAMMAR_DATA_COUNT = 0xff;
constexpr ui8 MAX_GRAMMAR_RULE_COUNT = 6;
constexpr ui8 MAX_GRAMMAR_RULE_LENGTH = 5;

struct CityGrammarRule {
    ui8 numStates;
    ui8 statesOffsets[MAX_GRAMMAR_RULE_LENGTH];
};

class BuildingGrammar {
public:
    void buildFromStrings(const Array<nString>& strings);
    // Input/output vector will be presized
    void buildRoomGraph(OUT std::vector<RoomNode>& graph) const;

private:
    // Array index is state index
    ui8 mNumRules = 0;
    CityGrammarRule mRules[MAX_GRAMMAR_RULE_COUNT];
    ui8 mData[MAX_GRAMMAR_DATA_COUNT];
};

//a - a | bc | bcb
//0 255 1 2 255 1 2 1 255