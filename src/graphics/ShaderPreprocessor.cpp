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
                i--;
                continue;
            } else if (defines && tryParseIfdef(input, i, *defines)) {
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
                i--;
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

bool vg::ShaderPreprocessor::tryParseInclude(nString& s, size_t i) {
    size_t startI = i;
    static const char INCLUDE_STR[10] = "#include";
    // Check that #include is correct
    for (int j = 0; INCLUDE_STR[j] != '\0'; j++) {
        if (s[i] == '\0') { return false; }
        if (s[i++] != INCLUDE_STR[j]) { return false; }
    }

    skipWhitespace(s, i);
    if (s[i] == '\0') { return false; }

    if (s[i++] != '\"') { return false; }
    // Grab the include string
    char includePathBuffer[512];
    int includeStrIndex = 0;
    while (s[i] != '\"' && s[i] != '\n' && includeStrIndex < 512) {
        // Check for invalid characters in path
        if (isWhitespace(s[i])) { return false; }
        includePathBuffer[includeStrIndex++] = s[i++];
    }
    if (s[i] != '\"') return false;
    includePathBuffer[includeStrIndex++] = '\0';

    std::string_view includePathSV(includePathBuffer, includeStrIndex);

    if (includeStrIndex) {

        nString includePath(includePathSV);
        if (m_parsedIncludes.find(includePath) != m_parsedIncludes.end()) {
            onError(ShaderPreprocessError("Circular include detected: " + nString(includePathBuffer), s.substr(0, i)));
            return false;
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
            return true;
        } else {
            onError(ShaderPreprocessError("Failed to open file " + nString(includePathSV), s.substr(0, i)));
            m_parsedIncludes.insert(std::move(includePath));
            return false;
        }

    }
    return false;
}

bool vorb::graphics::ShaderPreprocessor::tryParseIfdef(nString& s, size_t& i, const ShaderDefinesVector& defines) {
    size_t startI = i;
    static const char IFDEF_STR[7] = "#ifdef";
    for (int j = 0; IFDEF_STR[j] != '\0'; j++) {
        if (s[i] == '\0') return false;
        if (s[i++] != IFDEF_STR[j]) return false;
    }

    skipWhitespace(s, i);
    if (s[i] == '\0') return false;

    char defineNameBuffer[512];
    int defineNameIndex = 0;
    while (i < s.size() && !isWhitespace(s[i]) && defineNameIndex < 512) {
        defineNameBuffer[defineNameIndex++] = s[i++];
    }

    const std::string_view defineSV(defineNameBuffer, defineNameIndex);
    nString defineName(defineSV);

    auto it = std::find_if(defines.begin(), defines.end(), [&](const ShaderDefine& d) { return d.name == defineName; });
    const bool defineExists = it != defines.end() && it->active;

    // Find the matching #endif
    size_t endifPos = s.find("#endif", i);
    if (endifPos == nString::npos) {
        onError(ShaderPreprocessError("Missing #endif for #ifdef " + defineName, s.substr(0, i)));
        return false;
    }

    if (!defineExists) {
        // Remove the entire #ifdef block if the define doesn't exist
        s.erase(startI, endifPos + 6 - startI);
        i = startI - 1;  // Reprocess from the start of the modified content
    }
    else {
        // Remove just the #ifdef and #endif lines
        s.erase(endifPos, 6);
        s.erase(startI, i - startI);
        i = startI - 1;  // Reprocess from the start of the modified content
    }

    return true;
}
