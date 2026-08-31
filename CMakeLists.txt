# ============================================================================
# krkrlite —— KiriKiri 轻量级运行时
#
# 设计原则：
#   - Core/ 是只读子模块，所有平台差异代码放在 Patch/。
#   - 应用层只使用 EGL + GLES 接口；实现层（系统 EGL / ANGLE / SwiftShader）
#     由构建选项选择，运行时自动切换。
#   - 依赖全部通过 vcpkg manifest（vcpkg.json）声明。
# ============================================================================

cmake_minimum_required(VERSION 3.28)

# ----------------------------------------------------------------------------
# 0. vcpkg toolchain 自动探测
#    未显式指定 CMAKE_TOOLCHAIN_FILE 时，从 VCPKG_ROOT 环境变量探测。
#    未设置 VCPKG_ROOT 时，请显式传入：
#      -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
# ----------------------------------------------------------------------------
if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
    endif()
endif()

project(krkrlite C CXX)

# ----------------------------------------------------------------------------
# 1. 平台定义
#    _KRKRSDL3_* 是 Core 的平台条件宏（仅表示分支，不代表使用 SDL）。
# ----------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(_KRKRSDL3_LINUX=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_compile_definitions(_KRKRSDL3_ANDROID=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(_KRKRSDL3_WINDOWS=1)
endif()

# 公共功能宏（两个图形路径都需要）：
#   _KRKRSDL3_USE_FFMPEG —— 启用 Core 内置 FFmpeg 视频播放器（krmovie）。
add_compile_definitions(_KRKRSDL3_USE_FFMPEG=1)

# ----------------------------------------------------------------------------
# 2. 图形后端选项
#    - 原生路径：系统 EGL + GLES3（阶段一）
#    - AnyGL 路径：ANGLE 提供 EGL/GLES，实现层后端可多选并存，
#      运行时自动选择（见 Patch/environ/PlatformApp.cpp）
# ----------------------------------------------------------------------------
option(KRKR_USE_EGL_GLES      "Use the Patch EGL + GLES platform path" OFF)
option(KRKR_USE_ANYGL         "Use ANGLE/AnyGL EGL + GLES libraries" OFF)
option(KRKR_ANYGL_VULKAN      "Enable AnyGL Vulkan renderer" OFF)
option(KRKR_ANYGL_OPENGL      "Enable AnyGL OpenGL renderer" ON)
option(KRKR_ANYGL_SWIFTSHADER "Enable SwiftShader Vulkan ICD" OFF)

# ----------------------------------------------------------------------------
# 3. 源文件收集
#    Core/ 只读源码按模块收集；Patch/ 为平台差异实现。
# ----------------------------------------------------------------------------
# 3.1 TJS2 脚本引擎
file(GLOB_RECURSE SRC_TJS2_LIST CONFIGURE_DEPENDS "Core/tjs2/*.cpp" "Core/tjs2/*.c")

# 3.2 Core 主体（归档/主控/媒体/消息/脚本/工具 + 全部插件）
file(GLOB_RECURSE SRC_CORE_MAIN_LIST CONFIGURE_DEPENDS
    "Core/core/archive/*.cpp" "Core/core/archive/*.c"
    "Core/core/main/*.cpp" "Core/core/main/*.c"
    "Core/core/media/*.cpp" "Core/core/media/*.c"
    "Core/core/msg/*.cpp" "Core/core/msg/*.c"
    "Core/core/script/*.cpp" "Core/core/script/*.c"
    "Core/core/utils/*.cpp" "Core/core/utils/*.c"
    "Core/plugins/*.cpp" "Core/plugins/**/*.cpp"
    "Core/plugins/*.c" "Core/plugins/**/*.c")

# 3.3 渲染层（合成器 + 图层系统；渲染后端在 Patch/render/）
set(SRC_CORE_RENDER_LIST
     Core/core/render/gl/blend_function.cpp
     Core/core/render/gl/tvpgl.cpp
     Core/core/render/ComplexRect.cpp
     Core/core/render/DrawDevice.cpp
     Core/core/render/LayerBitmap.cpp
     Core/core/render/LayerManager.cpp
     Core/core/render/LayerTreeOwner.cpp
     Core/core/render/RenderManager.cpp
     Core/core/render/TVPColor.cpp
     Core/core/render/TVPTrans.cpp
     Core/core/render/TVPCompositor.cpp)

# 3.4 平台差异实现（Patch/）与 Core 的 POSIX 支撑
set(SRC_ENVIRON_LIST
    Patch/environ/Platform.cpp
    Patch/environ/PlatformFile.cpp
    Patch/environ/PlatformThread.cpp
    Patch/environ/PlatformUtil.cpp
    Patch/environ/PlatformAudio.cpp
    Patch/render/GLESRenderBackend.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SRC_ENVIRON_LIST
        Patch/environ/LinuxCore.cpp
        Core/environ/unix/unix_file.cpp
        Core/environ/unix/unix_sysinfo.cpp)
endif()

# 3.5 非 SDL 的 Linux 图形入口（仅原生/AnyGL 路径需要）
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (KRKR_USE_EGL_GLES OR KRKR_USE_ANYGL))
    list(APPEND SRC_ENVIRON_LIST Patch/environ/PlatformApp.cpp Patch/environ/PlatformEntry.cpp)
endif()

# ----------------------------------------------------------------------------
# 4. 目标创建
# ----------------------------------------------------------------------------
set(KRKR_SRC ${SRC_TJS2_LIST} ${SRC_CORE_MAIN_LIST} ${SRC_CORE_RENDER_LIST} ${SRC_ENVIRON_LIST})
if(CMAKE_SYSTEM_NAME MATCHES "Android|Emscripten")
    add_library(${PROJECT_NAME} SHARED ${KRKR_SRC})
else()
    add_executable(${PROJECT_NAME} ${KRKR_SRC})
endif()

# ----------------------------------------------------------------------------
# 5. 头文件路径
#    Core 模块目录 + Patch（平台差异）+ vcpkg 安装目录。
# ----------------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PUBLIC
    Core/core/archive Core/core/main Core/core/media/font Core/core/media/image
    Core/core/media/movie Core/core/media/sound Core/core/msg Core/core/render
    Core/core/script Core/core/utils Core/core/utils/math Core/environ Core/plugins Core/tjs2
    Patch Patch/render
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/plutovg)

# ----------------------------------------------------------------------------
# 6. 依赖（vcpkg manifest 声明，按用途分组）
# ----------------------------------------------------------------------------
# 6.1 媒体：视频解码（FFmpeg）、图像编解码（PNG/JPEG/WebP）、音频（Vorbis/Opus）
find_package(FFMPEG REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})
find_package(PNG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE PNG::PNG)
find_package(JPEG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE JPEG::JPEG)
find_package(libjpeg-turbo CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>)
find_package(WebP CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE WebP::webp WebP::webpdecoder WebP::webpdemux)
find_package(Vorbis CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vorbis::vorbis Vorbis::vorbisfile Vorbis::vorbisenc)
find_package(Opus CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Opus::opus)
find_package(OpusFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpusFile::opusfile)

# 6.2 文字与矢量：FreeType 字体、PlutoVG 矢量绘图
find_package(Freetype REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Freetype::Freetype)
find_package(plutovg CONFIG REQUIRED)
find_package(SQLite3 REQUIRED) # sqlite3 插件（xp3_vfs）
target_link_libraries(${PROJECT_NAME} PRIVATE plutovg::plutovg SQLite3::SQLite3)

# 6.3 正则与数学
find_package(oniguruma CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE oniguruma::onig)
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm-header-only)

# ----------------------------------------------------------------------------
# 7. 图形平台分支
#    - AnyGL：ANGLE 提供 EGL/GLES，实现后端编译进 ANGLE，运行时自动选择
#    - 原生：系统 EGL + GLESv2
# ----------------------------------------------------------------------------
if(KRKR_USE_ANYGL)
    find_package(Threads REQUIRED)

    # 7.1 后端编译标记：哪些实现后端被编译进 ANGLE（运行时候选列表据此生成）
    target_compile_definitions(${PROJECT_NAME} PRIVATE KRKR_USE_ANYGL=1)
    if(KRKR_ANYGL_VULKAN OR KRKR_ANYGL_SWIFTSHADER)
        target_compile_definitions(${PROJECT_NAME} PRIVATE KRKR_ANYGL_HAS_VULKAN=1)
    endif()
    if(KRKR_ANYGL_SWIFTSHADER)
        target_compile_definitions(${PROJECT_NAME} PRIVATE KRKR_ANYGL_HAS_SWIFTSHADER=1)
    endif()
    if(NOT DEFINED KRKR_ANYGL_OPENGL OR KRKR_ANYGL_OPENGL)
        target_compile_definitions(${PROJECT_NAME} PRIVATE KRKR_ANYGL_HAS_OPENGL=1)
    endif()

    # 7.2 Vulkan 头：系统无 SDK 头时使用 AnyGL/swiftshader 自带的 Vulkan 头
    if(KRKR_ANYGL_VULKAN)
        if(NOT Vulkan_INCLUDE_DIR)
            set(Vulkan_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/AnyGL/swiftshader/include")
        endif()
        find_package(Vulkan REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
    endif()

    # 7.3 ANGLE 子工程：编译 libEGL.so / libangle_glesv2_shared.so
    find_package(glfw3 CONFIG REQUIRED)
    add_subdirectory(AnyGL ${CMAKE_CURRENT_BINARY_DIR}/AnyGL EXCLUDE_FROM_ALL)
    target_link_libraries(${PROJECT_NAME} PRIVATE angle_egl_shared angle_glesv2_shared glfw Threads::Threads ${CMAKE_DL_LIBS})

elseif(KRKR_USE_EGL_GLES)
    # 原生路径：系统 EGL + GLESv2 + GLFW（阶段一，已验证）
    find_package(Threads REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE KRKR_NATIVE_EGL_GLES=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw EGL GLESv2 Threads::Threads ${CMAKE_DL_LIBS})
endif()
