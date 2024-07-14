#pragma once

#include <NsCore/Noesis.h>
#include <NsGui/CachedFontProvider.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
/// A font provider that searches fonts in local directories
////////////////////////////////////////////////////////////////////////////////////////////////////
class NoesisLocalFontProvider : public Noesis::CachedFontProvider
{
public:
    NoesisLocalFontProvider(const char* rootPath = "");

private:
    /// From CachedFontProvider
    //@{
    void ScanFolder(const Noesis::Uri& folder) override;
    Noesis::Ptr<Noesis::Stream> OpenFont(const Noesis::Uri& folder,
        const char* filename) const override;
    //@}

    void ScanFolder(const char* path, const Noesis::Uri& folder, const char* ext);

private:
    char mRootPath[512];
};