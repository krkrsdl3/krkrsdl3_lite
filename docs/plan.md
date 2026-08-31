# krkrlite 多图形后端实施记录

## 目标与约束

- 保持 `Core/` 原样，不在 Core 中提交或写入差异代码。
- 所有平台/后端差异放在 `Patch/`，由顶层 CMake 选择并引用。
- `AnyGL/` 顶层工程允许修改，用于适配和构建编排；`AnyGL/angle/` 与
  `AnyGL/swiftshader/` 是只读供应商源码，绝不修改其中任何文件。
- 禁止接入 SDL3（窗口、渲染、音频）；平台图形路径只使用 EGL + GLES。
- 最终以 `<game>/` 作为运行验证目标。

## 阶段一：EGL + GLES 原生路径 ✅

- `Patch/` 建立独立平台层：GLFW/X11 窗口、EGL display/context/surface、GLES3 绘制、
  输入事件转发、miniaudio 音频、POSIX 文件桥、系统字体兜底资源加载。
- 应用层渲染后端仅保留 `GLRenderBackend.cpp`。
- **验证**：完整启动进入游戏（XP3 7878+ 文件、startup.tjs、KAG 脚本、标题画面
  black/circle_logo/logo 加载、BGM/雪景/按钮），主循环稳定；输入可点击按钮；
  窗口关闭、全屏、modal 对话框均正常。

## 阶段二：AnyGL（GL / GLES / Vulkan）✅

- ANGLE 同时编译所有平台支持的后端（OpenGL/GLX、GLES 转发、Vulkan/XCB），
  运行时用 `eglGetPlatformDisplayEXT` 候选列表 `[opengl, gles, vulkan]`
  依次尝试自动选择；`KRKR_EGL_BACKEND` 可固定。
- **验证**：三后端均稳定进入标题画面（截图 mean≈0.99、色彩正常）。

## 阶段三：ANGLE Vulkan + SwiftShader 软渲染 ✅

- SwiftShader 构建出 `libvk_swiftshader.so` + ICD json；`ANGLE_VK_SWIFTSHADER_ICD_JSON`
  宏自动启用，无需手动设置 `VK_ICD_FILENAMES`。
- **验证**：`KRKR_EGL_BACKEND=vulkan` 稳定运行，`OpenGL Vendor` 显示
  `SwiftShader Device (LLVM 10.0.0)`（确认软渲染而非硬件驱动），
  标题画面完整渲染（截图 mean=0.954、32886 色），0 shader 错误。
- 注意：与阶段二同样显示 `backend 'vulkan'`；区别看 `OpenGL Vendor`
  （SwiftShader Device vs RADV）。

## 关键修复记录（详见 DEBUGGING.md）

- GCC 16 无法编译 ANGLE（`StrictNumeric`）→ 用 clang 构建。
- `eglGetPlatformDisplayEXT` 签名为 `EGLint*`，且必须传
  `EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE = EGL_PLATFORM_X11_EXT`；
  GLES 后端还需 `EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE`。
- Vulkan 黑屏（internal shader 解压 -2）→ 宿主 zlib 符号插值冲突，
  `angle_glesv2_shared` 加 `-Wl,-Bsymbolic-functions` 修复。
- VMA 3.4+ 断言 → AnyGL 层 `vma_wrapper.cpp` 提供 proc 地址（volk）。
- 视频音频卡顿 → 回调 trylock + 环形缓冲；modal 帧循环与主循环分离
  （SDL3 语义）；窗口关闭走主窗口（SDL_EVENT_QUIT 语义）。

## 已知限制

- 音频 pan 未实现（volume 可用）。
- 存档兼容：旧引擎生成的 `savedata/datasu.ksd` 可能触发脚本异常，删除后可正常。
- Wayland 会话需 XWayland（窗口路径为 X11）。
