#include <vector>
#include <cstdio>
#include <string>
#include <filesystem>
#include <unistd.h>
#include "TVPApplication.h"
#include "TVPSettings.h"
#include "PlatformView.h"
#include "PlatformThread.h"
#include "TVPCompositor.h"
#include "GLESRenderBackend.h"

bool TVPInitEGLGLESPlatform();
void TVPShutdownEGLGLESPlatform();
void TVPPollEventsBackend();
int TVPDrawSceneOnce(int interval);

int main(int argc, char** argv)
{
    if (!TVPInitEGLGLESPlatform()) return 1;
    char cwd[4096];
    if (argc >= 2 && argv[1][0] != '/' && getcwd(cwd, sizeof(cwd))) {
        std::string absolute = std::filesystem::path(cwd) / argv[1];
        argv[1] = strdup(absolute.c_str());
    }
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <game-directory-or-data.xp3>\n", argv[0]);
        TVPShutdownEGLGLESPlatform();
        return 2;
    }
    if (!TVPParseArguments(argc, argv)) {
        TVPShutdownEGLGLESPlatform();
        return 1;
    }
    // Create the GLES compositor backend before starting the game.
    krkrsdl3::TVPSetRenderBackend(new krkrsdl3::GLESRenderBackend());
    krkrsdl3::fetchGLInfo();
    Application = new tTVPApplication();
    const bool started = Application->StartApplication();
    // Main loop mirrors SDL3's SDL_AppIterate: one engine step + one
    // render + one event poll per iteration; VSync limits the rate via
    // eglSwapInterval. Modal window loops (ShowWindowAsModal) drive
    // themselves with TVPDrawSceneOnce, so no throttling here.
    int renderW = 1280, renderH = 720;
    while (started && !Application->IsTarminate())
    {
        if (!Application->Run())
            break;
        TVPGetWindowSize(&renderW, &renderH);
        krkrsdl3::TVPRenderOnce(renderW, renderH);
        TVPPollEventsBackend();
    }
    Application->OnExit();
    // Backend must be destroyed while the GL context is still current.
    krkrsdl3::TVPShutdownRenderBackend();
    delete Application;
    Application = nullptr;
    TVPShutdownEGLGLESPlatform();
    return 0;
}
