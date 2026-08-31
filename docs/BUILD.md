# krkrlite 构建与运行说明

本文档说明 krkrlite 的构建配置、后端选择与运行方式。
排错经验与调试技巧见 `DEBUGGING.md`；三阶段实施记录见 `plan.md`。
所有平台差异代码位于 `Patch/`；`Core/` 保持原样；`AnyGL/angle/` 与
`AnyGL/swiftshader/` 是只读供应商源码，只有 `AnyGL/CMakeLists.txt`
与 AnyGL 顶层新增文件（如 `vma_wrapper.cpp`）可修改。

## 0. 差分编译（重要）

- **不要删除构建目录**：ninja 的增量状态在 `.ninja_log`/`.ninja_deps`，
  删除后触发全量重编（约 8-10 分钟）。
- 改单个 Patch 文件后增量构建只需几十秒：
  `cmake --build build-xxx -j$(nproc)`
- 不要并发对同一构建目录跑多个 `cmake --build`（会损坏 ninja 状态）。
- 修改 `AnyGL/CMakeLists.txt` 的编译宏会触发 ANGLE 目标全量重编。

## 1. 环境要求

- Linux + X11（Wayland 会话下需 XWayland）；CMake ≥ 3.20、Ninja
- **clang**（ANGLE 编译）：GCC 16 无法编译该 ANGLE 版本（`StrictNumeric`
  模板推导错误，vendor 代码不可改），请用 clang 构建 AnyGL 路径
- vcpkg：`<vcpkg-root>`（manifest 依赖见 `vcpkg.json`）
- 系统库：EGL、GLESv2（原生路径）、X11；Vulkan 运行库（可选，Vulkan 后端用）

vcpkg manifest 依赖：

```
oniguruma zlib ffmpeg libpng libwebp libjpeg-turbo libvorbis opusfile
freetype plutovg glm pthreads glfw3 miniaudio
```

## 2. 构建配置

统一配置命令模板（原生路径与 AnyGL 路径共用 toolchain 参数）：

```bash
CONFIG="-DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_OVERLAY_PORTS=$PWD/vcpkg/ports \
        -DVCPKG_OVERLAY_TRIPLETS=$PWD/vcpkg/triplets \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
```

### 2.1 原生 EGL + GLES（阶段一）

```bash
cmake -S . -B build-native -G Ninja $CONFIG -DKRKR_USE_EGL_GLES=ON
cmake --build build-native -j$(nproc)
```

### 2.2 AnyGL（阶段二：GL / GLES / Vulkan 自动选择）

ANGLE 同时编译所有平台支持的后端，运行时按 `opengl → gles → vulkan`
顺序依次尝试（详见下文"运行时后端选择"）：

```bash
cmake -S . -B build-anygl-vk -G Ninja $CONFIG \
  -DKRKR_USE_ANYGL=ON -DKRKR_ANYGL_VULKAN=ON
cmake --build build-anygl-vk -j$(nproc)
```

### 2.3 AnyGL + SwiftShader（阶段三：无 GPU 软渲染）

```bash
cmake -S . -B build-anygl-sw -G Ninja $CONFIG \
  -DKRKR_USE_ANYGL=ON -DKRKR_ANYGL_SWIFTSHADER=ON -DKRKR_ANYGL_VULKAN=ON
cmake --build build-anygl-sw -j$(nproc)
# SwiftShader ICD 是独立 target，需显式构建一次：
cmake --build build-anygl-sw --target swiftshader_vulkan -j$(nproc)
```

### 2.4 CMake 选项

| 选项 | 含义 | 默认 |
| --- | --- | --- |
| `KRKR_USE_EGL_GLES` | 原生系统 EGL + GLES3 路径 | OFF |
| `KRKR_USE_ANYGL` | AnyGL/ANGLE 提供 EGL/GLES | OFF |
| `KRKR_ANYGL_OPENGL` | 编译 OpenGL 实现后端（与 Vulkan 可并存） | ON |
| `KRKR_ANYGL_VULKAN` | 编译 Vulkan 实现后端 | OFF |
| `KRKR_ANYGL_SWIFTSHADER` | 构建 SwiftShader ICD 并自动启用软渲染 | OFF |

应用层**只保留一个 GLES 渲染后端**（`Core/core/render/backend/GLRenderBackend.cpp`）；
ANGLE/SwiftShader 只是 EGL/GLES 接口之下的实现层。

## 3. 运行时后端选择

AnyGL 构建默认自动选择后端；启动日志打印实际选中项：

```
[anygl] getPlatformDisplayEXT=0x... std=0x...
ANGLE display created with backend 'opengl' (EGL 1.5)
```

- 自动顺序：`opengl → gles → vulkan`（SwiftShader 构建：`vulkan` 优先）
- 环境变量 `KRKR_EGL_BACKEND=gl|gles|vulkan` 可固定首个候选（调试/指定用）：

```bash
KRKR_EGL_BACKEND=vulkan ./build-anygl-vk/krkrlite <game.xp3>
```

- SwiftShader 构建无需设置 `VK_ICD_FILENAMES`（编译宏自动指向生成的 ICD）。
- **确认确实走了软渲染**：启动日志的 `OpenGL Vendor` 应包含
  `SwiftShader Device (LLVM ...)`；若显示 `RADV`/`AMD Radeon` 则是硬件驱动
  （阶段三未生效）。
- 手动备选（不使用编译宏时）：`VK_ICD_FILENAMES=<build-anygl-sw>/bin/swiftshader/vk_swiftshader_icd.json KRKR_EGL_BACKEND=vulkan`

## 4. 运行

```bash
# 参数为游戏目录或 data.xp3 的路径（启动方式与原版一致）
./build-native/krkrlite <game>/data.xp3
```

- 日志输出到 stderr 与 `savedata/krkr.console.log`。
- 测试游戏：`<game>/`。

## 5. 目录与文件

```
Patch/environ/
  PlatformEntry.cpp    入口：参数解析、GL 后端安装、主循环（SDL_AppIterate 模式）
  PlatformApp.cpp      GLFW/X11 窗口、EGL/GLES 初始化、事件转发、全屏/关闭、
                       create_platform_display（ANGLE 后端候选自动选择）、
                       TVPDrawSceneOnce（modal 循环帧入口）
  Platform.cpp         CPU 检测、TVPAbortApplication 等
  PlatformUtil.cpp     非 SDL 工具、TVPGetRoughTickCount、消息框、日志
  PlatformFile.cpp     POSIX 文件桥（目录遍历全部容错）
  PlatformThread.cpp   sleep 等线程工具
  PlatformAudio.cpp    miniaudio 播放后端（环形缓冲、回调 trylock）
  LinuxCore.cpp        GetResourceStream（系统字体兜底）、OS 名
CMakeLists.txt         顶层构建：源列表、依赖、后端选项
vcpkg.json             manifest 依赖（含 miniaudio）
```

关键编译定义（由顶层 CMake 注入）：

- `KRKR_NATIVE_EGL_GLES=1`：原生系统 EGL/GLES 路径
- `KRKR_USE_ANYGL=1` / `KRKR_ANYGL_HAS_VULKAN=1` / `KRKR_ANYGL_HAS_OPENGL=1` /
  `KRKR_ANYGL_HAS_SWIFTSHADER=1`：AnyGL 后端候选编译标记
- `_KRKRSDL3_USE_FFMPEG=1`：启用 Core 自带 FFmpeg 视频播放器（krmovie）
- `_KRKRSDL3_LINUX=1`：Core 平台条件（仅表示 Linux 分支，不涉及 SDL）

## 6. 提交约定

- 构建目录（`build-*`）已在 `.gitignore` 排除。
- `Core/` 与 `AnyGL/angle/`、`AnyGL/swiftshader/` 不允许改动。
- `AnyGL/` 是独立 Git 仓库，其提交只允许触及顶层 `AnyGL/CMakeLists.txt`
  与 AnyGL 顶层新增文件。
