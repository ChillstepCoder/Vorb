#include "Vorb/stdafx.h"
#include "Vorb/graphics/ShaderParser.h"

#include <sstream>

#include "Vorb/io/IOManager.h"

// Static definitions
eventpp::CallbackList<void(const nString&)> vg::ShaderParser::onParseError;
std::set<nString> vg::ShaderParser::m_parsedIncludes;
bool vg::ShaderParser::isNormalComment = false;
bool vg::ShaderParser::isBlockComment = false;
vio::IOManager* vg::ShaderParser::ioManager;

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

void vg::ShaderParser::parseVertexShader(const cString inputCode, OUT nString& resultCode, vio::IOManager& iom) {
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
        }
        resultCode += c;
    }
}

void vg::ShaderParser::parseFragmentOrGeometryShader(const cString inputCode, OUT nString& resultCode, vio::IOManager& iom) {
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
        if (!isComment() && c == '#') {
            if (tryParseInclude(input, i)) {
                i--;
                continue;
            }
        } 
        resultCode += c;
    }
}

bool vg::ShaderParser::checkForComment(const cString s, size_t i) {
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

bool vg::ShaderParser::tryParseInclude(nString& s, size_t i) {
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
    nString include = "";
    while (s[i] != '\"' && s[i] != '\n') {
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
            onParseError("Circular include detected: " + nString(includePathBuffer));
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
            onParseError("Failed to open file " + nString(includePathSV));
            m_parsedIncludes.insert(std::move(includePath));
            return false;
        }

    }
    return false;
}
