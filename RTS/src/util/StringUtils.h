#pragma once

namespace Utils {
    namespace String
    {
        bool equalsIgnoreCase(const std::string_view a, const std::string_view b);
        std::string& toLower(std::string& string);
        std::string toLowerCopy(std::string_view string);
        void erase(std::string& str, const char* chars);
        void erase(std::string& str, const std::string& chars);
        std::string subStr(const std::string& string, size_t offset, size_t count = std::string::npos);
        std::string trimWhitespace(const std::string& str);
    }

    std::string_view getFilename(const std::string_view filepath);
    std::string getFilename(const std::string& filepath);
    std::string getExtension(const std::string& filename);
    std::string removeExtension(const std::string& filename);

    // Keeps delimiters except for spaces, used for shaders
    std::vector<std::string> splitStringAndKeepDelims(std::string str);

    std::vector<std::string> splitString(const std::string_view string, const std::string_view& delimiters);
    std::vector<std::string> splitString(const std::string_view string, const char delimiter);
    // Insert delimiter before each upper case character.
    std::string splitAtUpperCase(std::string_view string, std::string_view delimiter = " ", bool ifLowerCaseOnTheRight = true);
    std::string toLower(const std::string_view& string);
    std::string toUpper(const std::string_view& string);
    std::string bytesToString(uint64_t bytes);

    int SkipBOM(std::istream& in);
    std::string ReadFileAndSkipBOM(const std::filesystem::path& filepath);

    template<class...Durations, class DurationIn>
    std::tuple<Durations...> BreakDownDuration(DurationIn d) {
        std::tuple<Durations...> retval;
        using discard = int[];
        (void)discard {
            0, (void((
                (std::get<Durations>(retval) = std::chrono::duration_cast<Durations>(d)),
                (d -= std::chrono::duration_cast<DurationIn>(std::get<Durations>(retval)))
                )), 0)...
        };
        return retval;
    }

    std::string DurationToString(std::chrono::duration<double> duration);

    template <typename IsAlreadyUsedFn>
    static std::string AddSuffixToMakeUnique(const std::string& name, IsAlreadyUsedFn&& isUsed)
    {
        auto nameToUse = name;
        int suffix = 1;

        while (isUsed(nameToUse))
            nameToUse = name + "_" + std::to_string(++suffix);

        return nameToUse;
    }

    // Get<float> -> Get (Float)
    std::string TemplateToParenthesis(std::string_view name);
    // Useful for 'Described' types to display type name in GUI or to serialize.
    // SoundGraph::Get<float> -> Get (Float)
    std::string CreateUserFriendlyTypeName(std::string_view name);

    //==============================================================================
    /// constexpr utilities

    constexpr bool StartsWith(std::string_view t, std::string_view s)
    {
        auto len = s.length();
        return t.length() >= len && t.substr(0, len) == s;
    }

    constexpr bool EndsWith(std::string_view t, std::string_view s)
    {
        auto len1 = t.length(), len2 = s.length();
        return len1 >= len2 && t.substr(len1 - len2) == s;
    }

    constexpr size_t GetNumberOfTokens(std::string_view source, std::string_view delimiter)
    {
        size_t count = 1;
        auto pos = source.begin();
        while (pos != source.end())
        {
            if (std::string_view(&*pos, delimiter.size()) == delimiter)
                ++count;

            ++pos;
        }
        return count;
    }

    template<size_t N>
    constexpr std::array<std::string_view, N> splitString(std::string_view source, std::string_view delimiter)
    {
        std::array<std::string_view, N> tokens;

        auto tokenStart = source.begin();
        auto pos = tokenStart;

        size_t i = 0;

        while (pos != source.end())
        {
            if (std::string_view(&*pos, delimiter.size()) == delimiter)
            {
                tokens[i] = std::string_view(&*tokenStart, (pos - tokenStart));
                tokenStart = pos += delimiter.size();
                ++i;
            }
            else
            {
                ++pos;
            }
        }

        if (pos != source.begin())
            tokens[N - 1] = std::string_view(&*tokenStart, (pos - tokenStart));

        return tokens;
    }

    constexpr std::string_view RemoveNamespace(std::string_view name)
    {
        const auto pos = name.find_last_of(':');
        if (pos == std::string_view::npos)
            return name;

        return name.substr(name.find_last_of(':') + 1);
    }

    constexpr std::string_view RemoveOuterNamespace(std::string_view name)
    {
        const auto first = name.find_first_of(':');
        if (first == std::string_view::npos)
            return name;

        if (first < name.size() - 1 && name[first + 1] == ':')
            return name.substr(first + 2);
        else
            return name.substr(first + 1);
    }

    template<size_t N>
    constexpr std::array<std::string_view, N>& RemoveNamespace(std::array<std::string_view, N>& memberList)
    {
        for (std::string_view& fullName : memberList)
            fullName = RemoveNamespace(fullName);

        return memberList;
    }
}
