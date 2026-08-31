#include "tjsCommHead.h"
#include "Platform.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

void TVPConsoleLog(const tjs_char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

int tTVPScreen::GetWidth() { return 1280; }
int tTVPScreen::GetHeight() { return 720; }
int tTVPScreen::GetDesktopLeft() { return 0; }
int tTVPScreen::GetDesktopTop() { return 0; }
int tTVPScreen::GetDesktopWidth() { return GetWidth(); }
int tTVPScreen::GetDesktopHeight() { return GetHeight(); }

ttstr TVPGetPlatformName() { return ttstr(TJS_N("GLFW/EGL/GLES")); }
std::string TVPGetCurrentLanguage() { return "en_ANY"; }
void TVPOpenPatchLibUrl() {}
tjs_uint64 TVPGetRoughTickCount()
{
    using clock = std::chrono::steady_clock;
    return (tjs_uint64)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
}

int TVPShowSimpleMessageBox(const ttstr&, const ttstr&) { return 0; }
int TVPShowSimpleMessageBox(const char*, const char*, unsigned int, const char**) { return 0; }
int TVPShowSimpleMessageBox(const ttstr&, const ttstr&, const std::vector<ttstr>&) { return 0; }
std::string TVPShowFileSelector(const std::string&, const std::string& filename, std::string, bool) { return filename; }
std::string TVPShowDirectorySelector(const std::string&, std::string initdir, std::string) { return initdir; }
int TVPShowSimpleInputBox(ttstr&, const ttstr&, const ttstr&, const std::vector<ttstr>&) { return 0; }
void TVPShowIME(int, int, int, int) {}
void TVPHideIME() {}
