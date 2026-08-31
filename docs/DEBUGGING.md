# krkrlite 调试指南

本文档沉淀 krkrlite 移植过程中的排错经验，供后续排查参考。
配合 `BUILD.md` 使用。

## 1. 快速构建（差分编译）

- **不要 `rm -rf` 构建目录**：ninja 依赖 `.ninja_log`/`.ninja_deps` 做增量
  判断，删除后会把所有输出视为脏，触发全量重编（约 8-10 分钟）。
- **不要并发构建同一目录**：多个 `cmake --build` 同时跑会损坏 ninja
  状态（`premature end of file`、`RefreshDyndepDependents` 断言）。
  恢复方法：先删损坏的 `*.dd`/`*.ddi`（dyndep 生成物），再构建。
  若 `.ninja_log` 已损坏丢失，只能接受一次全量重编。
- 改单个 Patch 文件后增量构建只需几十秒：
  ```bash
  cmake --build build-anygl-vk -j$(nproc)
  ```
- 顶层 CMake 使用 `file(GLOB ... CONFIGURE_DEPENDS)`，修改 CMakeLists
  或新增源文件会触发重新 configure（几秒），但不会重编未变化的源。
- clang 编译命令带 `-fmodules-ts -fdeps-*`（GCC 模块扫描参数），
  clang 单文件测试时需去掉这些参数。

## 2. 运行时后端选择

AnyGL 模式下 ANGLE 同时编译了多个实现后端，运行时按候选顺序
依次尝试（自动选择），可用环境变量固定顺序：

```bash
# 默认顺序：opengl -> gles -> vulkan（SwiftShader 构建：vulkan 优先）
./build-anygl-vk/krkrlite <game.xp3>

# 强制指定后端（调试用）
KRKR_EGL_BACKEND=gl     ./build-anygl-vk/krkrlite <game.xp3>
KRKR_EGL_BACKEND=gles   ./build-anygl-vk/krkrlite <game.xp3>
KRKR_EGL_BACKEND=vulkan ./build-anygl-vk/krkrlite <game.xp3>
```

启动日志会打印实际选中的后端：
```
[anygl] getPlatformDisplayEXT=0x... std=0x...
ANGLE display created with backend 'opengl' (EGL 1.5)
```

### 2.1 ANGLE 平台显示创建的关键点

- **必须用 `eglGetPlatformDisplayEXT`（而非 `eglGetDisplay`）**：
  `eglGetDisplay(x11_display)` 在 ANGLE 下会走 EGL 转发平台
  （DisplayEGL → 系统 Mesa EGL → 可能崩溃在 Wayland 探测）。
- **EXT 入口签名是 `const EGLint*`**（32 位属性数组），标准
  `eglGetPlatformDisplay` 是 `const EGLAttrib*`（64 位）。用错签名
  会导致属性错位、`EGL_BAD_ATTRIBUTE (0x3004)`。两个入口都通过
  `eglGetProcAddress` 获取，优先 EXT。
- **必须传 `EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE =
  EGL_PLATFORM_X11_EXT`**：否则 ANGLE 的
  `CreateDisplayFromAttribs` 找不到 X11 实现（GLX / DisplayVkXcb），
  返回 `EGL_NO_DISPLAY`。
- **GLES 转发后端**（`EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE`）
  需要额外传 `EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE =
  EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE`，否则在纯 X11（未定义
  GBM/WAYLAND）下同样拿不到实现。
- 失败排查顺序：`eglGetError()` → `EGL_KHR_debug` 回调
  （`eglDebugMessageControlKHR`）→ gdb 断点
  `ValidateGetPlatformDisplayCommon`（验证返回 false 的具体分支）。

## 3. ANGLE 编译问题（经验）

- **GCC 16 无法编译 ANGLE**：`StrictNumeric` 转换模板在 `new[]`
  边界上下文的推导失败（`IntermNode.cpp`），`-fpermissive` 无效，
  vendor 代码不可改 → **用 clang 编译**（`-DCMAKE_C_COMPILER=clang
  -DCMAKE_CXX_COMPILER=clang++`）。
- **GL 后端源**不在 `angle-srcs.cmake` 的清单里，需手动收集
  `renderer/gl/*.cpp` + `egl/` + `glx/` 源，并定义
  `ANGLE_ENABLE_OPENGL`、`ANGLE_ENABLE_GL_DESKTOP_BACKEND`、
  `ANGLE_ENABLE_ESSL`、`ANGLE_ENABLE_GLSL`。
- **GLSL translator 缺源**：`ExpandFragmentOutputsToVec4.cpp`、
  `WrapStructConstructors.cpp` 在清单中只有 .h，需手动补 .cpp。
- **Vulkan 后端链接**需要 wayland-client、gbm、SPIRV-Tools
  （`swiftshader/third_party/SPIRV-Tools` 可 add_subdirectory）。
- **VMA 3.4+ 动态函数断言**：ANGLE 的 vendor wrapper 不提供
  `vkGetInstanceProcAddr`/`vkGetDeviceProcAddr`，运行时断言崩溃。
  解法：在 AnyGL 层复制 wrapper（`AnyGL/vma_wrapper.cpp`），补两个
  函数指针（由 volk 提供，volk.h 中为全局变量），并在 CMake 里
  替换 vendor wrapper 源。`VMA_STATIC_VULKAN_FUNCTIONS` 不可行
  （VK_NO_PROTOTYPES 下无全局 vk 符号）。
- **非 Windows 强制关闭 DirectX 后端**，否则会编译 `tchar.h` 相关的
  D3D 源。
- 系统无 Vulkan SDK 头时，用 AnyGL 自带
  `swiftshader/include` 作为 `Vulkan_INCLUDE_DIR`。
- **SwiftShader 自动启用依赖完整 ICD 加载链**（缺少任一环都会静默回退到硬件驱动）：
  1. 应用侧 Vulkan 候选必须带 `EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE`
     （否则 ANGLE 默认 HARDWARE，日志仍显示 RADV）
  2. ANGLE 必须编译 `ANGLE_ENABLE_SWIFTSHADER`（否则该 deviceType 报
     EGL_BAD_ATTRIBUTE 被验证拒绝）
  3. `ANGLE_VK_SWIFTSHADER_ICD_JSON` 宏值必须是**相对模块目录**的路径
     （ANGLE 会拼接到 libangle_glesv2_shared.so 所在目录；绝对路径拼接后失效）
  4. ICD manifest 必须由 `file(WRITE)` 生成（`-E echo` + shell 重定向会
     剥掉 JSON 引号）
  5. manifest 的 `library_path` 必须是**绝对路径**（loader 不按 json 目录
     解析相对路径）
  - 手动备选（免编译宏）：`VK_ICD_FILENAMES=<icd.json> KRKR_EGL_BACKEND=vulkan`
  - 确认真的用了软渲染：日志 `OpenGL Vendor` 显示
    `SwiftShader Device (LLVM ...)` 而非 `RADV`。
- **Vulkan 后端黑屏 + `Failure to decompressed internal shader: -2`**：
  所有 gzip 数据本身有效、单独编译 zlib 测试也正常，但进程内
  ANGLE 的压缩 shader 解压失败——这是 **zlib 符号插值**：宿主
  可执行文件静态链接的 zlib 与 ANGLE 库内嵌的 chromium zlib 符号
  冲突（`GzipUncompressHelper` 与 `inflate*` 解析到不同实现）。
  修复：给 `angle_glesv2_shared` 加链接选项
  `-Wl,-Bsymbolic-functions`，让库内部函数调用绑定自身定义。
- 判断后端是否真的渲染：`import -window <id> shot.png` 后看
  `mean`/`colors`，纯黑 = mean≈0 且 colors=1。

## 4. 运行验证技巧

### 4.1 X11 交互模拟（无真实输入时）

```bash
# 模拟鼠标点击（左键 button=1，右键 button=3）
gcc /tmp/simclick.c -o /tmp/simclick -lXtst -lX11
/tmp/simclick <x> <y> [button]

# 模拟点击窗口关闭按钮（发送 WM_DELETE_WINDOW）
gcc /tmp/simclose.c -o /tmp/simclose -lX11
/tmp/simclose <窗口标题子串>
```

窗口位置用 `xwininfo -root -tree | grep -i krkrlite` 获取，
模拟点击的屏幕坐标 = 窗口偏移 + 游戏内坐标。

### 4.2 画面验证（无头/远程排查）

```bash
WINID=$(xwininfo -root -tree | grep -iE 'krkrlite|人類' | awk '{print $1}')
import -window "$WINID" /tmp/shot.png
convert /tmp/shot.png -colorspace gray -format "mean=%[fx:mean] colors=%k\n" info:
# mean≈0 + colors=1 → 纯黑屏（渲染/后端问题）
# mean≈0.9 + colors>10000 → 正常画面
```

### 4.3 最小复现（隔离 ANGLE 问题）

写独立小程序直接链接 `build-*/AnyGL/libEGL.so`，快速验证 EGL 调用：

```bash
clang egltest.c -o /tmp/egltest \
  -I AnyGL/angle/include -I AnyGL/swiftshader/include \
  -L build-anygl-vk/AnyGL -lEGL -lX11 \
  -Wl,-rpath,$PWD/build-anygl-vk/AnyGL
```

### 4.4 gdb 要点

- 动态库符号：`set breakpoint pending on` 才能断到
  `libangle_glesv2_shared.so` 里的函数。
- 无调试信息时用 `finish` + 寄存器（bool 返回值看 `$al`）。
- `EGL_KHR_debug` 回调（`eglDebugMessageControlKHR`）能拿到
  ANGLE 验证失败的具体错误消息，比猜 `eglGetError` 值快得多。

## 5. 运行时环境变量速查

| 变量 | 作用 |
| --- | --- |
| `KRKR_EGL_BACKEND=gl\|gles\|vulkan` | 固定 AnyGL 首个后端候选（调试/指定） |
| `VK_ICD_FILENAMES=<json>` | 手动指定 Vulkan ICD（SwiftShader 构建已自动设置） |
| `VK_LOADER_DEBUG=error,warn` | Vulkan loader 诊断输出（扩展/层加载失败排查） |
| `ANGLE_DEFAULT_PLATFORM` | ANGLE 默认平台（仅当应用未显式指定类型时生效） |

## 6. 验证矩阵（三后端 + 软渲染）

| 构建 | 后端 | 验证方式 | 通过标志 |
| --- | --- | --- | --- |
| build-native | 系统 EGL+GLES | 运行 + 日志 | `Program started on Nyarch Linux (GLFW/EGL/GLES)` |
| build-anygl-vk | opengl | 运行 + 截图 | `ANGLE display created with backend 'opengl'`，截图 mean≈0.9 |
| build-anygl-vk | gles | `KRKR_EGL_BACKEND=gles` | 同上，backend 'gles' |
| build-anygl-vk | vulkan | `KRKR_EGL_BACKEND=vulkan` | 同上，backend 'vulkan'，无 `internal shader` 错误 |
| build-anygl-sw | vulkan+SwiftShader | 运行 + 截图 | backend 'vulkan'，`OpenGL Vendor` 含 `SwiftShader Device`，截图 mean≈0.95、色数 >10000 |

## 7. 常见症状速查

| 症状 | 可能原因 | 排查方向 |
| --- | --- | --- |
| 黑屏但日志推进 | 渲染后端未设置 / native window 传参错误 / shader 缺失 | 截图验证；查 `ANGLE display created` 日志；查 `internal shader` 错误 |
| 窗口关闭按钮异常 | GLFW close 回调关的是 active（modal）窗口而非主窗口 | 应关 `TVPMainWindow` 并撤销 GLFW close 请求 |
| 全屏切换后画面异常 | EGL surface 未跟随窗口尺寸 | `TVPGetWindowSize` 同步 winWidth/winHeight |
| 启动即 `EGL_NO_DISPLAY` | 平台显示属性缺失/错误 | 见 2.1，加 fprintf 逐候选打印 `eglGetError` |
| 音频卡顿 | 回调线程阻塞（锁竞争）| 回调必须 trylock 非阻塞，环形缓冲 O(1) 读写 |
| `Filesystem error` 崩溃 | 目录遍历未容错 | 所有 directory_iterator 用 error_code 版本 |

## 8. 平台层结构速记

```
PlatformEntry.cpp    入口：参数解析、GL 后端安装、主循环（SDL_AppIterate 模式）
PlatformApp.cpp      窗口/EGL/GLES/输入/全屏/关闭 + TVPDrawSceneOnce（modal 用）
                     create_platform_display：ANGLE 后端候选自动选择
PlatformAudio.cpp    miniaudio 回调式播放（环形缓冲 + trylock）
LinuxCore.cpp        资源加载（系统字体兜底）、OS 名
```

主循环与 modal 循环是**两个独立的帧入口**：
- 主循环：`Run() → TVPRenderOnce → TVPPollEventsBackend()`（每帧）
- modal 循环（`ShowWindowAsModal`）：`TVPDrawSceneOnce(interval)`（节流版，
  内部完成 Run + 渲染 + 事件轮询，返回剩余毫秒）
