#pragma once
class GLExtensions
{
public:
    void init();
    bool hasExtension(const char* extension);

    static std::set<nString> sExtensions;
};

extern GLExtensions sGlExtensions;