# krkrlite

> **⚠️ 免责声明**
>
> 本项目的核心目的是**评估与验证 krkrsdl3 代码库的 AI 友好程度**，
> 以及 AI 辅助下跨平台图形栈迁移的可行性。项目仅在 ArchLinux 环境下
> 完成构建与运行验证，**尚不具备实际使用价值**，请勿将其应用于
> 生产环境或日常使用。

krkrlite 是一个基于 **EGL + OpenGL ES** 构建的轻量级 KiriKiri（吉里吉里）运行时。
应用层只使用 EGL/GLES 接口；图形实现层按平台能力自动选择：

| 场景 | 实现路径 | 后端 |
| --- | --- | --- |
| 支持 EGL+GLES 的平台 | 原生路径（`KRKR_USE_EGL_GLES`） | 系统 EGL + GLES3 |
| 需要 ANGLE 的平台 | AnyGL 路径（`KRKR_USE_ANYGL`） | OpenGL / GLES 转发 / Vulkan 自动选择 |
| 无 GPU 平台 | AnyGL + SwiftShader | Vulkan 软渲染（`KRKR_ANYGL_SWIFTSHADER`） |

## 特性

- **渲染**：GLFW/X11 窗口，EGL display/context/surface，GLES3 合成（`GLRenderBackend`），
  VSync 限帧；全屏切换
- **AnyGL 多后端**：ANGLE 同时编译 OpenGL（GLX）、GLES 转发（系统 EGL）、Vulkan（XCB）
  三个实现，运行时依次尝试自动选择；`KRKR_EGL_BACKEND=gl|gles|vulkan` 可固定后端
- **无 GPU 软渲染**：SwiftShader Vulkan ICD 由构建生成，运行时自动启用
- **音频**：miniaudio 回调式播放（环形缓冲、非阻塞锁），支持 BGM/SE/视频音轨
- **输入**：鼠标/键盘/滚轮/文本事件经 GLFW 转发到引擎（KAG 按钮、右键菜单、modal 窗口均可交互）
- **视频**：Core 内置 FFmpeg 播放器路径（`_KRKRSDL3_USE_FFMPEG`）
- **资源**：XP3 归档、插件（7z/minizip、sqlite3、PlutoVG 等）完整接入

## 环境要求

- Linux + X11（Wayland 会话需 XWayland）；CMake ≥ 3.20、Ninja、clang（ANGLE 编译）
- vcpkg：`<vcpkg-root>`（manifest 依赖见 `vcpkg.json`，含 miniaudio）
- 系统库：EGL、GLESv2、X11；Vulkan 运行库（可选，用于 Vulkan 后端）

## 快速开始

```bash
# 阶段一：原生 EGL+GLES
cmake --build build-native -j$(nproc)
./build-native/krkrlite <game-data.xp3>

# 阶段二：AnyGL（GL/GLES/Vulkan 自动选择）
cmake --build build-anygl-vk -j$(nproc)
KRKR_EGL_BACKEND=vulkan ./build-anygl-vk/krkrlite <game-data.xp3>   # 可选固定后端

# 阶段三：SwiftShader 软渲染（无 GPU 环境）
cmake --build build-anygl-sw -j$(nproc)
cmake --build build-anygl-sw --target swiftshader_vulkan -j$(nproc)
KRKR_EGL_BACKEND=vulkan ./build-anygl-sw/krkrlite <game-data.xp3>
```

运行参数为游戏目录或 `data.xp3` 的路径（与原版一致）。日志输出到 stderr
与 `savedata/krkr.console.log`。

> 完整构建配置、后端矩阵与运行说明见 [docs/BUILD.md](docs/BUILD.md)。

## CMake 选项

| 选项 | 含义 | 默认 |
| --- | --- | --- |
| `KRKR_USE_EGL_GLES` | 原生系统 EGL + GLES3 路径 | OFF |
| `KRKR_USE_ANYGL` | AnyGL/ANGLE 提供 EGL/GLES | OFF |
| `KRKR_ANYGL_OPENGL` | 编译 OpenGL 实现后端 | ON |
| `KRKR_ANYGL_VULKAN` | 编译 Vulkan 实现后端 | OFF |
| `KRKR_ANYGL_SWIFTSHADER` | 构建 SwiftShader ICD 并启用软渲染 | OFF |

## 目录结构

```
Patch/          平台差异代码（窗口/EGL/GLES/输入/音频/文件/资源），Core 保持原样
AnyGL/          ANGLE + SwiftShader 构建编排（顶层 CMakeLists 可改，vendor 只读）
Core/           KiriKiri 核心（只读，git 子模块）
docs/           文档（构建、调试、实施计划）
vcpkg.json      manifest 依赖（含 miniaudio）
```

## 文档

- [docs/BUILD.md](docs/BUILD.md) —— 构建配置、后端选择、差分编译
- [docs/DEBUGGING.md](docs/DEBUGGING.md) —— 排错经验、调试技巧、症状速查
- [docs/plan.md](docs/plan.md) —— 三阶段实施计划与验证记录
- [docs/AI-FRIENDLY.md](docs/AI-FRIENDLY.md) —— AI 友好型代码特点总结（本项目实践经验）

## 约束

- `Core/` 与 `AnyGL/angle/`、`AnyGL/swiftshader/` 是只读源码，绝不修改。
- 应用层只保留一个 GLES 渲染后端；SDL 不参与窗口/渲染/音频。
- 依赖通过 `vcpkg.json` 声明；系统平台库（EGL/GLES/X11）除外。
