#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
// Xlib (pulled in by glfw3native.h) defines macros that collide with the
// Core enums (None, Bool, ...); neutralize them before Core headers.
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif
#include <EGL/egl.h>
#ifdef KRKR_USE_ANYGL
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#endif
#include <GLES3/gl3.h>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include "tjsCommHead.h"
#include "PlatformView.h"
#include "Platform.h"
#include "TVPApplication.h"
#include "WindowManager.h"
#include "TVPWindow.h"
#include "TVPCompositor.h"
#include "tvpinputdefs.h"

static GLFWwindow* g_window = nullptr;
static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;
static int winWidth = 1280, winHeight = 720;
static int winRestoreX = 0, winRestoreY = 0, winRestoreW = 1280, winRestoreH = 720;
static std::string g_title = "krkrlite";

static void destroy_graphics()
{
    if (g_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
        if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
        eglTerminate(g_display);
    }
    g_context = EGL_NO_CONTEXT;
    g_surface = EGL_NO_SURFACE;
    g_display = EGL_NO_DISPLAY;
    if (g_window) glfwDestroyWindow(g_window);
    g_window = nullptr;
    glfwTerminate();
}

bool TVPInitEGLGLESPlatform();
void TVPShutdownEGLGLESPlatform();
void TVPInstallInputCallbacks();
int TVPConvertKeyCodeToVKCode(int keyCode);

//---------------------------------------------------------------------------
// In AnyGL mode the EGL implementation is ANGLE. All platform-supported
// backends are compiled in; pick one at runtime by trying candidates in
// order: native OpenGL (GLX), then the GLES-forwarding backend (ANGLE
// routes through the system EGL), then Vulkan. SwiftShader builds prefer
// the software Vulkan backend, so it goes first there.
//---------------------------------------------------------------------------
static EGLDisplay create_platform_display(EGLint* outMajor, EGLint* outMinor)
{
#ifdef KRKR_USE_ANYGL
    // eglGetPlatformDisplayEXT takes EGLint attributes; the EGL 1.5
    // eglGetPlatformDisplay takes EGLAttrib attributes. Use the EXT entry
    // with its exact signature.
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplayStd =
        (PFNEGLGETPLATFORMDISPLAYPROC)eglGetProcAddress("eglGetPlatformDisplay");
    std::fprintf(stderr, "[anygl] getPlatformDisplayEXT=%p std=%p\n",
                 (void*)getPlatformDisplay, (void*)getPlatformDisplayStd);

    // Candidate backend types, tried in order. KRKR_EGL_BACKEND=gl|gles|vulkan
    // pins the first candidate for testing; otherwise the automatic order is
    // OpenGL -> GLES-forwarding -> Vulkan (SwiftShader prefers Vulkan).
    struct Candidate
    {
        EGLint type;       // EGL_PLATFORM_ANGLE_TYPE_ANGLE value; 0 = ANGLE default
        EGLint deviceType; // EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE value; 0 = unset
    };
    Candidate candidates[4];
    int n = 0;
    const char* forced = std::getenv("KRKR_EGL_BACKEND");
#ifdef KRKR_ANYGL_HAS_SWIFTSHADER
    // SwiftShader 构建：Vulkan 候选显式请求软渲染设备（否则 ANGLE 默认
    // 使用硬件 ICD，日志里仍是 RADV 等真实驱动）。
    if (!forced || !std::strcmp(forced, "vulkan"))
        candidates[n++] = {EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE,
                           EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE};
#endif
#ifdef KRKR_ANYGL_HAS_OPENGL
    if (!forced || !std::strcmp(forced, "gl"))
        candidates[n++] = {EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE, 0};
    if (!forced || !std::strcmp(forced, "gles"))
        candidates[n++] = {EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE,
                           EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE};
#endif
#ifdef KRKR_ANYGL_HAS_VULKAN
    if (!forced || !std::strcmp(forced, "vulkan"))
        candidates[n++] = {EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, 0};
#endif

    for (int i = 0; i < n; ++i)
    {
        EGLDisplay dpy = EGL_NO_DISPLAY;
        const char* typeName = "default";
        if (candidates[i].type == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE) typeName = "opengl";
        else if (candidates[i].type == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE) typeName = "gles";
        else if (candidates[i].type == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE) typeName = "vulkan";
        if (getPlatformDisplay)
        {
            EGLint attrs[6];
            int a = 0;
            if (candidates[i].type != 0)
                attrs[a++] = EGL_PLATFORM_ANGLE_TYPE_ANGLE, attrs[a++] = candidates[i].type;
            if (candidates[i].deviceType != 0)
                attrs[a++] = EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, attrs[a++] = candidates[i].deviceType;
            // X11 native platform: ANGLE selects the X11-capable display
            // implementation (GLX / DisplayVkXcb / system-EGL surface).
            attrs[a++] = EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, attrs[a++] = EGL_PLATFORM_X11_EXT;
            attrs[a] = EGL_NONE;
            dpy = getPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, glfwGetX11Display(), attrs);
        }
        else if (getPlatformDisplayStd)
        {
            EGLAttrib attrs[6];
            int a = 0;
            if (candidates[i].type != 0)
                attrs[a++] = EGL_PLATFORM_ANGLE_TYPE_ANGLE, attrs[a++] = candidates[i].type;
            if (candidates[i].deviceType != 0)
                attrs[a++] = EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, attrs[a++] = candidates[i].deviceType;
            attrs[a++] = EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE, attrs[a++] = EGL_PLATFORM_X11_EXT;
            attrs[a] = EGL_NONE;
            dpy = getPlatformDisplayStd(EGL_PLATFORM_ANGLE_ANGLE, glfwGetX11Display(), attrs);
        }
        else
        {
            dpy = eglGetDisplay((EGLNativeDisplayType)glfwGetX11Display());
        }
        if (dpy == EGL_NO_DISPLAY)
        {
            std::fprintf(stderr, "[anygl] candidate '%s' -> no display (eglError=%d)\n",
                         typeName, (int)eglGetError());
            continue;
        }
        EGLint major = 0, minor = 0;
        if (eglInitialize(dpy, &major, &minor))
        {
            TVPConsoleLog("ANGLE display created with backend '%s' (EGL %d.%d)",
                          typeName, (int)major, (int)minor);
            if (outMajor) *outMajor = major;
            if (outMinor) *outMinor = minor;
            return dpy;
        }
        TVPConsoleLog("ANGLE backend '%s' init failed, trying next", typeName);
        eglTerminate(dpy);
    }
    return EGL_NO_DISPLAY;
#else
    EGLDisplay dpy = eglGetDisplay((EGLNativeDisplayType)glfwGetX11Display());
    if (dpy != EGL_NO_DISPLAY)
    {
        EGLint major = 0, minor = 0;
        if (eglInitialize(dpy, &major, &minor))
        {
            if (outMajor) *outMajor = major;
            if (outMinor) *outMinor = minor;
            return dpy;
        }
        eglTerminate(dpy);
    }
    return EGL_NO_DISPLAY;
#endif
}

bool TVPInitEGLGLESPlatform()
{
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    g_window = glfwCreateWindow(1280, 720, g_title.c_str(), nullptr, nullptr);
    if (!g_window) { destroy_graphics(); return false; }
    TVPInstallInputCallbacks();
    EGLint major = 0, minor = 0;
    g_display = create_platform_display(&major, &minor);
    if (g_display == EGL_NO_DISPLAY) {
        destroy_graphics(); return false;
    }
    const EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    EGLConfig config = nullptr; EGLint count = 0;
    if (!eglChooseConfig(g_display, config_attrs, &config, 1, &count) || count != 1 ||
        !eglBindAPI(EGL_OPENGL_ES_API)) { destroy_graphics(); return false; }
    const EGLint context_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, context_attrs);
    g_surface = eglCreateWindowSurface(g_display, config,
        (EGLNativeWindowType)glfwGetX11Window(g_window), nullptr);
    if (g_context == EGL_NO_CONTEXT || g_surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
        destroy_graphics(); return false;
    }
    eglSwapInterval(g_display, 1); // VSync, same as the ohos entry point
    return true;
}

void TVPShutdownEGLGLESPlatform() { destroy_graphics(); }
bool TVPSoftwareRenderBackendAvailable() { return false; }
std::vector<std::string> TVPListAllRenderBackend() { return {"opengl"}; }
void TVPCreateTextureBackend(TVPSprite&) {}
void TVPUpdateTextureBackend(TVPSprite*, uint8_t*, int, int, int) {}
void TVPDestroyTextureBackend(TVPSprite*) {}
void TVPRenderTextureBackend(TVPSprite*, int, int, int, int) {}
void TVPRenderClearBackend() { glClear(GL_COLOR_BUFFER_BIT); }
void TVPRenderPresentBackend() { TVPSwapBuffersBackend(); }
void TVPSwapBuffersBackend() { if (g_display != EGL_NO_DISPLAY) eglSwapBuffers(g_display, g_surface); }
void TVPSetWindowTitle(const char* title) { if (title) { g_title = title; if (g_window) glfwSetWindowTitle(g_window, title); } }
std::string TVPGetWindowTitle() { return g_title; }
void TVPSetWindowFullscreen(bool isFull)
{
    if (!g_window)
        return;
    if (isFull)
    {
        glfwGetWindowPos(g_window, &winRestoreX, &winRestoreY);
        glfwGetWindowSize(g_window, &winRestoreW, &winRestoreH);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(g_window, monitor, 0, 0,
                                 mode ? mode->width : winRestoreW,
                                 mode ? mode->height : winRestoreH,
                                 mode ? mode->refreshRate : GLFW_DONT_CARE);
        }
    }
    else
    {
        glfwSetWindowMonitor(g_window, nullptr, winRestoreX, winRestoreY,
                             winRestoreW, winRestoreH, GLFW_DONT_CARE);
    }
}
void TVPGetWindowSize(int* w, int* h) { if (g_window) glfwGetWindowSize(g_window, &winWidth, &winHeight); if (w) *w = winWidth; if (h) *h = winHeight; }
void TVPGetWindowSizeInPixels(int* w, int* h) { TVPGetWindowSize(w, h); }
void TVPSetWindowSize(int w, int h) { if (g_window) glfwSetWindowSize(g_window, w, h); }

// Event poll used by the main loop (SDL3's SDL_AppIterate counterpart).
// GLFW callbacks fire during this poll. Window close is handled by the
// on_window_close callback, which mirrors SDL3's SDL_EVENT_QUIT handling
// (TVPGetActiveWindow()->Close, but targeting the main window so closing
// the OS window exits the app even while a modal owns the active slot).
void TVPPollEventsBackend()
{
    glfwPollEvents();
}

int TVPDrawSceneOnce(int interval)
{
    // SDL3 TVPDrawSceneOnce semantics, used by modal window loops
    // (TVPWindow::ShowWindowAsModal): throttled full frame — advance the
    // engine, render, poll events. Returns 0 when a frame was processed,
    // or the remaining milliseconds until the next frame is due.
    static tjs_uint64 lastTick = TVPGetRoughTickCount();
    tjs_uint64 curTick = TVPGetRoughTickCount();
    int remain = interval - (curTick - lastTick);
    if (remain <= 0)
    {
        eglMakeCurrent(g_display, g_surface, g_surface, g_context);
        ::Application->Run();
        iTVPTexture2D::RecycleProcess();
        // 合成器完成渲染（清屏/绘制/呈现全部由当前渲染后端负责）
        krkrsdl3::TVPRenderOnce(winWidth, winHeight);
        TVPPollEventsBackend();
        lastTick = curTick;
        return 0;
    }
    else
    {
        return remain;
    }
}

//---------------------------------------------------------------------------
// Input event forwarding: GLFW callbacks -> krkrsdl3 event entry points.
//---------------------------------------------------------------------------
static void on_mouse_button(GLFWwindow*, int button, int action, int)
{
    tTVPMouseButton mb = mbLeft;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) mb = mbRight;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mb = mbMiddle;
    double x = 0, y = 0;
    glfwGetCursorPos(g_window, &x, &y);
    static long long evtCount = 0;
    TVPConsoleLog("[input] mouse button=%d action=%s at %d,%d (event#%lld)",
                  button, action == GLFW_PRESS ? "down" : "up", (int)x, (int)y, ++evtCount);
    if (action == GLFW_PRESS) krkrsdl3::KRKR_Trig_MouseDown(mb, (int)x, (int)y);
    else if (action == GLFW_RELEASE) krkrsdl3::KRKR_Trig_MouseUp(mb, (int)x, (int)y);
}
static void on_mouse_move(GLFWwindow*, double x, double y)
{
    krkrsdl3::KRKR_Trig_MouseMove((int)x, (int)y);
}
static void on_mouse_scroll(GLFWwindow*, double xoff, double yoff)
{
    double x = 0, y = 0;
    glfwGetCursorPos(g_window, &x, &y);
    krkrsdl3::KRKR_Trig_MouseScroll((int)xoff, (int)yoff, (int)x, (int)y);
}
static void on_key(GLFWwindow*, int key, int, int action, int)
{
    if (action == GLFW_PRESS) krkrsdl3::KRKR_Trig_KeyDown(key);
    else if (action == GLFW_RELEASE) krkrsdl3::KRKR_Trig_KeyUp(key);
}
static void on_char(GLFWwindow*, unsigned int codepoint)
{
    char buf[8] = {};
    int len = 0;
    if (codepoint < 0x80) { buf[0] = (char)codepoint; len = 1; }
    else if (codepoint < 0x800) { buf[0] = 0xC0 | (codepoint >> 6); buf[1] = 0x80 | (codepoint & 0x3F); len = 2; }
    else if (codepoint < 0x10000) { buf[0] = 0xE0 | (codepoint >> 12); buf[1] = 0x80 | ((codepoint >> 6) & 0x3F); buf[2] = 0x80 | (codepoint & 0x3F); len = 3; }
    else { buf[0] = 0xF0 | (codepoint >> 18); buf[1] = 0x80 | ((codepoint >> 12) & 0x3F); buf[2] = 0x80 | ((codepoint >> 6) & 0x3F); buf[3] = 0x80 | (codepoint & 0x3F); len = 4; }
    krkrsdl3::KRKR_Trig_TextInput(std::string(buf, len));
}
static void on_window_close(GLFWwindow*)
{
    // SDL3 SDL_EVENT_QUIT counterpart: route through the engine's close
    // flow. Target the main window so the whole app exits; closing only
    // the active (modal) window would leave the app running with a dead
    // close request. The engine owns the final decision (onCloseQuery can
    // veto), so revoke GLFW's pending close request afterwards — the
    // engine terminates via TVPTerminated, not via a stuck ShouldClose.
    TVPWindow* win = TVPMainWindow ? TVPMainWindow : TVPGetActiveWindow();
    if (win) win->Close();
    if (g_window)
        glfwSetWindowShouldClose(g_window, GLFW_FALSE);
}
//---------------------------------------------------------------------------
// GLFW key -> Windows VK code. GLFW uses ASCII for letters/digits so those
// already match VK_*; special keys need an explicit table.
//---------------------------------------------------------------------------
int TVPConvertKeyCodeToVKCode(int keyCode)
{
    switch (keyCode)
    {
        case GLFW_KEY_ESCAPE: return VK_ESCAPE;
        case GLFW_KEY_ENTER: return VK_RETURN;
        case GLFW_KEY_TAB: return VK_TAB;
        case GLFW_KEY_BACKSPACE: return VK_BACK;
        case GLFW_KEY_INSERT: return VK_INSERT;
        case GLFW_KEY_DELETE: return VK_DELETE;
        case GLFW_KEY_RIGHT: return VK_RIGHT;
        case GLFW_KEY_LEFT: return VK_LEFT;
        case GLFW_KEY_DOWN: return VK_DOWN;
        case GLFW_KEY_UP: return VK_UP;
        case GLFW_KEY_PAGE_UP: return VK_PRIOR;
        case GLFW_KEY_PAGE_DOWN: return VK_NEXT;
        case GLFW_KEY_HOME: return VK_HOME;
        case GLFW_KEY_END: return VK_END;
        case GLFW_KEY_CAPS_LOCK: return VK_CAPITAL;
        case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: return VK_SHIFT;
        case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL: return VK_CONTROL;
        case GLFW_KEY_LEFT_ALT: case GLFW_KEY_RIGHT_ALT: return VK_MENU;
        case GLFW_KEY_LEFT_SUPER: case GLFW_KEY_RIGHT_SUPER: return 0x5B; // VK_LWIN
        case GLFW_KEY_MENU: return 0x5D; // VK_APPS
        case GLFW_KEY_KP_0: case GLFW_KEY_KP_1: case GLFW_KEY_KP_2: case GLFW_KEY_KP_3:
        case GLFW_KEY_KP_4: case GLFW_KEY_KP_5: case GLFW_KEY_KP_6: case GLFW_KEY_KP_7:
        case GLFW_KEY_KP_8: case GLFW_KEY_KP_9:
            return VK_NUMPAD0 + (keyCode - GLFW_KEY_KP_0);
        case GLFW_KEY_KP_ADD: return VK_ADD;
        case GLFW_KEY_KP_SUBTRACT: return VK_SUBTRACT;
        case GLFW_KEY_KP_MULTIPLY: return VK_MULTIPLY;
        case GLFW_KEY_KP_DIVIDE: return VK_DIVIDE;
        case GLFW_KEY_KP_DECIMAL: return VK_DECIMAL;
        case GLFW_KEY_KP_ENTER: return VK_RETURN;
        default: break;
    }
    if (keyCode >= GLFW_KEY_F1 && keyCode <= GLFW_KEY_F12)
        return VK_F1 + (keyCode - GLFW_KEY_F1);
    if (keyCode >= 32 && keyCode < 127)
        return keyCode; // ASCII letters/digits/punctuation match VK values
    return 0;
}
//---------------------------------------------------------------------------
void TVPInstallInputCallbacks()
{
    glfwSetMouseButtonCallback(g_window, on_mouse_button);
    glfwSetCursorPosCallback(g_window, on_mouse_move);
    glfwSetScrollCallback(g_window, on_mouse_scroll);
    glfwSetKeyCallback(g_window, on_key);
    glfwSetCharCallback(g_window, on_char);
    glfwSetWindowCloseCallback(g_window, on_window_close);
}
