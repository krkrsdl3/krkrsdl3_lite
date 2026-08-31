#include "SampleApplication.h"
#include "util/shader_utils.h"

#include <cstdio>

namespace
{
void PrintRuntimeInfo(EGLDisplay display)
{
    const char *vendor   = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    const char *version  = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    const char *sl       = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    const char *eglVendor = display != EGL_NO_DISPLAY ? eglQueryString(display, EGL_VENDOR) : nullptr;
    const char *eglVersion = display != EGL_NO_DISPLAY ? eglQueryString(display, EGL_VERSION) : nullptr;
    const char *eglExtensions = display != EGL_NO_DISPLAY ? eglQueryString(display, EGL_EXTENSIONS) : nullptr;

    std::printf("[ANGLE runtime] EGL_VENDOR: %s\n", eglVendor ? eglVendor : "<null>");
    std::printf("[ANGLE runtime] EGL_VERSION: %s\n", eglVersion ? eglVersion : "<null>");
    std::printf("[ANGLE runtime] GL_VENDOR: %s\n", vendor ? vendor : "<null>");
    std::printf("[ANGLE runtime] GL_RENDERER: %s\n", renderer ? renderer : "<null>");
    std::printf("[ANGLE runtime] GL_VERSION: %s\n", version ? version : "<null>");
    std::printf("[ANGLE runtime] GLSL_VERSION: %s\n", sl ? sl : "<null>");
    std::printf("[ANGLE runtime] EGL_EXTENSIONS: %s\n", eglExtensions ? eglExtensions : "<null>");
}
}  // namespace

class ColorTriangle final : public SampleApplication
{
  public:
    ColorTriangle(int argc, char **argv) : SampleApplication("ANGLE Color Triangle", argc, argv), mNativeGL(false)
    {
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--use-gl=native") mNativeGL = true;
    }
    bool initialize() override
    {
        constexpr char vs[] = "attribute vec2 p; attribute vec3 c; varying vec3 v; void main(){gl_Position=vec4(p,0,1);v=c;}";
        constexpr char fs[] = "precision mediump float; varying vec3 v; void main(){gl_FragColor=vec4(v,1);}";
        mProgram = CompileProgram(vs, fs);
        PrintRuntimeInfo(mNativeGL ? EGL_NO_DISPLAY : getDisplay());
        glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
        return mProgram != 0;
    }
    void destroy() override { if (mProgram) glDeleteProgram(mProgram); }
    void draw() override
    {
        const GLfloat data[] = {0, .65f, 1,0,0, -.6f,-.55f, 0,1,0, .6f,-.55f, 0,0,1};
        glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());
        glClear(GL_COLOR_BUFFER_BIT); glUseProgram(mProgram);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,5*sizeof(GLfloat),data);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,5*sizeof(GLfloat),data+2);
        glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glDrawArrays(GL_TRIANGLES,0,3);
    }
  private:
    bool mNativeGL = false;
    GLuint mProgram = 0;
};
int main(int argc, char **argv) { ColorTriangle app(argc, argv); return app.run(); }
