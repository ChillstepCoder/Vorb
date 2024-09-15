#include "Vorb/stdafx.h"
#include "Vorb/graphics/ShaderPreprocessor.h"

#include <sstream>

#include "Vorb/io/IOManager.h"

// Static definitions
eventpp::CallbackList<void(const ShaderPreprocessError&)> vg::ShaderPreprocessor::onError;
std::set<nString> vg::ShaderPreprocessor::m_parsedIncludes;
bool vg::ShaderPreprocessor::isNormalComment = false;
bool vg::ShaderPreprocessor::isBlockComment = false;
vio::IOManager* vg::ShaderPreprocessor::ioManager;

// Checks if c is a whitespace char
inline bool isWhitespace(char c) {
    return (c == '\n' || c == '\0' || c == '\t' || c == '\r' || c == ' ');
}
// Skips all whitespace by incrementing i accordingly
inline void skipWhitespace(const nString& s, size_t& i) {
    while (isWhitespace(s[i]) && i < s.size()) i++;
}
// Checks if c is a number
inline bool isNumeric(char c) {
    return (c >= '0' && c <= '9');
}

void vg::ShaderPreprocessor::processVertexShader(const cString inputCode, OUT nString& resultCode, vio::IOManager& iom, const ShaderDefinesVector* defines) {
    isNormalComment = false;
    isBlockComment = false;
    m_parsedIncludes.clear();

    // Convert to nString for easy include replacements
    nString input(inputCode);
    ioManager = &iom;

    nString data;

    // Anticipate final code size
    resultCode = "";
    resultCode.reserve(input.size());

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];
        checkForComment(input.c_str(), i);
        if (!isComment() && c == '#' && (i == 0 || input[i - 1] == '\n')) {
            if (tryParseInclude(input, i)) {
                continue;
            }
            else if (defines && tryParseIfdef(input, i, *defines)) {
                continue;
            }
        }
        resultCode += c;
    }
}

void vg::ShaderPreprocessor::processFragmentOrGeometryShader(const cString inputCode, OUT nString& resultCode, vio::IOManager& iom, const ShaderDefinesVector* defines) {
    isNormalComment = false;
    isBlockComment = false;
    m_parsedIncludes.clear();

    // Convert to nString for easy include replacements
    nString input(inputCode);

    ioManager = &iom;

    nString data;

    // Anticipate final code size
    resultCode = "";
    resultCode.reserve(input.size());

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];
        checkForComment(input.c_str(), i);
        if (!isComment() && c == '#' && (i == 0 || input[i - 1] == '\n')) {
            if (tryParseInclude(input, i)) {
                continue;
            }
            else if (defines && tryParseIfdef(input, i, *defines)) {
                continue;
            }
        } 
        resultCode += c;
    }
}

bool vg::ShaderPreprocessor::checkForComment(const cString s, size_t i) {
    if (s[i] == '/' && s[i + 1] == '/') {
        isNormalComment = true;
        return true;
    } else if (s[i] == '\n') {
        isNormalComment = false;
    } else if (s[i] == '/' && s[i + 1] == '*') {
        isBlockComment = true;
        return true;
    } else if (s[i] == '*' && s[i + 1] == '/') {
        isBlockComment = false;
    }
    return false;
}

bool vg::ShaderPreprocessor::tryParseInclude(nString& s, size_t& i) {
    size_t startI = i;
    static const char INCLUDE_STR[10] = "#include";
    // Check that #include is correct
    for (int j = 0; INCLUDE_STR[j] != '\0'; j++) {
        if (s[i] == '\0') { i = startI; return false; }
        if (s[i++] != INCLUDE_STR[j]) { i = startI; return false; }
    }

    skipWhitespace(s, i);
    if (s[i] == '\0') { i = startI; return false; }

    if (s[i++] != '\"') { i = startI; return false; }
    // Grab the include string
    char includePathBuffer[512];
    int includeStrIndex = 0;
    while (s[i] != '\"' && s[i] != '\n' && includeStrIndex < 512) {
        // Check for invalid characters in path
        if (isWhitespace(s[i])) { i = startI; return false; }
        includePathBuffer[includeStrIndex++] = s[i++];
    }
    if (s[i] != '\"') { i = startI; return false; }
    includePathBuffer[includeStrIndex++] = '\0';

    std::string_view includePathSV(includePathBuffer, includeStrIndex);

    if (includeStrIndex) {

        nString includePath(includePathSV);
        if (m_parsedIncludes.find(includePath) != m_parsedIncludes.end()) {
            onError(ShaderPreprocessError("Circular include detected: " + nString(includePathBuffer), s.substr(0, i)));
            i = startI; return false;
        }
        // Replace the include with the file contents
        nString data = "";
        if (ioManager->readFileToString(vio::Path(includePathBuffer), data)) {
            if (data.empty()) {
                s.erase(startI, i + 1 - startI);
            } else {
                while (data.back() == '\0') data.pop_back();
                // Replace by erasing and inserting
                s.erase(startI, i + 1 - startI);
                if (data.length()) s.insert(startI, data.c_str());
            }

            m_parsedIncludes.insert(std::move(includePath));
            i = startI - 1;
            return true;
        } else {
            onError(ShaderPreprocessError("Failed to open file " + nString(includePathSV), s.substr(0, i)));
            m_parsedIncludes.insert(std::move(includePath));
            i = startI;
            return false;
        }

    }
    i = startI;
    return false;
}

bool vorb::graphics::ShaderPreprocessor::tryParseIfdef(nString& s, size_t& i, const ShaderDefinesVector& defines) {
    size_t startI = i;
    bool isNDef = false;

    // Check if we have enough characters for the shortest directive (#ifdef)
    if (i + 6 >= s.size()) {
        return false;
    }

    // Check for #ifdef or #ifndef
    if (s[i] == '#' && s[i + 1] == 'i' && s[i + 2] == 'f') {
        if (i + 6 < s.size() && s[i + 3] == 'd' && s[i + 4] == 'e' && s[i + 5] == 'f') {
            i += 6; // Move past "#ifdef"
        }
        else if (i + 7 < s.size() && s[i + 3] == 'n' && s[i + 4] == 'd' && s[i + 5] == 'e' && s[i + 6] == 'f') {
            i += 7; // Move past "#ifndef"
            isNDef = true;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }

    skipWhitespace(s, i);
    if (s[i] == '\0') { i = startI; return false; }

    char defineNameBuffer[512];
    int defineNameIndex = 0;
    while (i < s.size() && !isWhitespace(s[i]) && defineNameIndex < 512) {
        defineNameBuffer[defineNameIndex++] = s[i++];
    }

    const std::string_view defineSV(defineNameBuffer, defineNameIndex);
    nString defineName(defineSV);

    auto it = std::find_if(defines.begin(), defines.end(), [&](const ShaderDefine& d) { return d.name == defineName; });
    bool useFirstBlock;
    if (isNDef) {
        useFirstBlock = (it == defines.end() || !it->active);
    }
    else {
        useFirstBlock = (it != defines.end() && it->active);
    }

    // Find the matching #endif and potential #else
    size_t endifPos = s.find("#endif", i);
    size_t elsePos = s.find("#else", i);

    if (endifPos == nString::npos) {
        onError(ShaderPreprocessError("Missing #endif for #ifdef/#ifndef " + defineName, s.substr(0, i)));
        i = startI;
        return false;
    }

    if (elsePos != nString::npos && elsePos < endifPos) {
        // We have an #else clause
        if (useFirstBlock) {
            // Keep the first block, remove the #else and the second block
            s.erase(elsePos, endifPos + 6 - elsePos);
            s.erase(startI, i - startI);
        }
        else {
            // Remove the first block and the #else, keep the second block
            s.erase(endifPos, 6);
            s.erase(startI, elsePos + 5 - startI);
        }
    }
    else {
        // No #else clause
        if (!useFirstBlock) {
            // Remove the entire #ifdef/#ifndef block
            s.erase(startI, endifPos + 6 - startI);
        }
        else {
            // Remove just the #ifdef/#ifndef and #endif lines
            s.erase(endifPos, 6);
            s.erase(startI, i - startI);
        }
    }

    i = startI - 1;  // Reprocess from the start of the modified content
    return true;
}