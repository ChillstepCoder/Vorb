#include "stdafx.h"
#include "BuildingGrammar.h"

#include "Random.h"

constexpr ui8 STATE_SEPARATOR_CHAR = '|';

void BuildingGrammar::buildFromStrings(const Array<nString>& strings) {
    assert(strings.size() < 0xff); // Convert to ui8
    mNumRules = (ui8)strings.size();
    size_t dataIndex = 0;
    for (size_t i = 0; i < strings.size(); ++i) {
        const nString& str = strings[i];
        CityGrammarRule& rule = mRules[i];
        rule.numStates = 1;
        rule.statesOffsets[0] = (ui8)dataIndex;
        for (size_t j = 0; j < str.length(); ++j) {
            ui8 chr = str[j];
            if (chr == STATE_SEPARATOR_CHAR) {
                mData[dataIndex++] = GRAMMAR_SEPARATOR;
                rule.statesOffsets[rule.numStates++] = (ui8)dataIndex;
            }
            else if (chr == '0') {
                mData[dataIndex++] = GRAMMAR_DELIMINATOR;
            }
            else {
                // These should be all abc, ect
                ui8 val = chr - 'a';
                assert(val < MAX_GRAMMAR_RULE_COUNT);
                mData[dataIndex++] = val;
            }
        }

        // Bounds checking
        assert(dataIndex < 256);
        mData[dataIndex++] = GRAMMAR_DELIMINATOR;
    }
}

void BuildingGrammar::buildRoomGraph(OUT std::vector<RoomNode>& graph) const {
    ui8 ruleBuffer[255];
    const ui32 maxNodes = graph.size();
    RoomNodeID currentIndex = 0; // Start with door node
    ruleBuffer[currentIndex] = 0;
    ui32 ruleBufferCount = 1;
    ui32 totalNodes = 1;

    std::cout << "BUILD ROOM GRAPH ";
    while (currentIndex < totalNodes && totalNodes < maxNodes) {
        RoomNode& currentNode = graph[currentIndex];
        const ui32 currentRuleIndex = ruleBuffer[currentIndex];
        const CityGrammarRule& currentRule = mRules[currentRuleIndex];
        // Pick our next statec
        assert(currentRule.numStates);
        ui32 resultStateIndex = Random::xorshf96() % currentRule.numStates;
        // Build the new nodes from this grammar state
        const ui8 stateOffset = currentRule.statesOffsets[resultStateIndex];
        const ui8* newNodeType = &mData[stateOffset];

        // Add new rooms while we can
        while (*newNodeType != GRAMMAR_SEPARATOR && *newNodeType != GRAMMAR_DELIMINATOR && totalNodes < maxNodes) {
            // Generate a new node and make it a child
            currentNode.childRooms[currentNode.numChildren] = totalNodes;
            graph[totalNodes].parentRoom = currentIndex;
            ruleBuffer[ruleBufferCount++] = *newNodeType;
            ++currentNode.numChildren;
            ++totalNodes;
            ++newNodeType;
        }
        ++currentIndex;
    }

    // Shrink the graph if we didnt fill it up
    if (totalNodes < maxNodes) {
        graph.resize(totalNodes);
    }
}
