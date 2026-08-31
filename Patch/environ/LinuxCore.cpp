// Patch-level Linux core support replacing Core/environ/linux/linux_core.cpp.
// Differences from the Core version:
//   - GetResourceStream falls back to system fonts and returns nullptr
//     instead of throwing when a bundled resource is missing.
#include "tjsCommHead.h"
#include "Platform.h"
#include "PlatformFile.h"
#include "PlatformVideo.h"
#include "UtilStreams.h"
#include "TVPStorage.h"

#include <sys/utsname.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

//---------------------------------------------------------------------------
static tTVPMemoryStream* LoadLocalFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return nullptr;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0)
    {
        std::fclose(f);
        return nullptr;
    }
    tTVPMemoryStream* ret = new tTVPMemoryStream(nullptr, (tjs_uint)size);
    std::fread(ret->GetInternalBuffer(), 1, (size_t)size, f);
    std::fclose(f);
    return ret;
}

tTVPMemoryStream* GetResourceStream(const ttstr& filename)
{
    // bundled resource next to the executable: <exeDir>/Res/<filename>
    std::string base = TVPNativeExeDir.AsStdString();
    if (!base.empty() && base.back() != '/')
        base += '/';
    tTVPMemoryStream* s = LoadLocalFile(base + "Res/" + filename.AsStdString());
    if (s)
        return s;

    // fallback for the default fallback font: use a system CJK font
    static const char* kSystemFonts[] = {
        "/usr/share/fonts/TTF/odokai.ttf",
        "/usr/share/fonts/TTF/odosung.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto/NotoSansCJK-Regular.ttc",
        nullptr};
    if (filename == TJS_N("DroidSansFallback.ttf"))
    {
        for (int i = 0; kSystemFonts[i]; ++i)
        {
            tTVPMemoryStream* fs = LoadLocalFile(kSystemFonts[i]);
            if (fs)
                return fs;
        }
    }
    return nullptr;
}
//---------------------------------------------------------------------------
std::string TVPGetPackageVersionString()
{
    return "linux";
}
//---------------------------------------------------------------------------
ttstr TVPGetOSName()
{
    std::ifstream file("/etc/os-release");
    std::string line;
    std::string pretty_name;
    while (std::getline(file, line))
    {
        if (line.find("PRETTY_NAME=") == 0)
        {
            pretty_name = line.substr(12);
            if (!pretty_name.empty() && pretty_name.front() == '"')
                pretty_name = pretty_name.substr(1, pretty_name.size() - 2);
            break;
        }
    }
    if (!pretty_name.empty())
        return ttstr(pretty_name.c_str());
    struct utsname buffer;
    if (uname(&buffer) == 0)
        return ttstr((std::string(buffer.sysname) + " " + buffer.release).c_str());
    return ttstr(TJS_N("Linux"));
}
//---------------------------------------------------------------------------
void TVPInvokeMenu(int /*x*/, int /*y*/, void* /*_menu*/)
{
}
//---------------------------------------------------------------------------
// No system video player on this platform; the FFmpeg plugin path handles
// video. Returning nullptr makes callers fall back to the null players.
LayerVideoPlayer* CreateLayerVideoPlayer(TVPVideoEventCallback /*cb*/, void* /*cbctx*/)
{
    return nullptr;
}
OverlayVideoPlayer* CreateOverlayVideoPlayer(TVPVideoEventCallback /*cb*/, void* /*cbctx*/)
{
    return nullptr;
}
