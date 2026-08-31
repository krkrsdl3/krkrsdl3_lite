# AI 友好型代码特点（krkrlite 实践经验）

> 本项目的核心目的之一是**验证 krkrsdl3 的 AI 友好程度与 AI 辅助迁移的
> 可行性**。本文档基于本次"非 SDL 平台层迁移 + 多图形后端接入"的真实
> 经历，总结"AI 友好型代码"应当具备的特点，以及本次踩坑得到的反面教训。

## 1. 什么是 AI 友好型代码

AI 友好不是"给 AI 看"，而是**让代码库可以被大模型高效、正确地理解、
修改与迁移**。本质是**降低上下文理解成本**与**提高改动可预测性**：

- 一个文件/一段逻辑能被独立理解，不需要全局状态推理
- 改动一个点不会波及无关区域（可局部验证）
- 失败时能快速定位根因（可诊断）
- 文档与代码一致，不会误导

## 2. 代码层面的特点

### 2.1 清晰的模块边界与职责单一

- 平台层与引擎层彻底解耦：引擎只依赖接口（`iTVPRenderBackend`、
  `iTVPSoundBuffer`、`iTVPStorageMedia`），平台实现可整体替换
- 差异代码集中管理：本项目所有平台差异收进 `Patch/`，引擎（`Core/`）
  零改动即可换图形栈（系统 EGL → ANGLE → SwiftShader）
- 每个文件职责单一：`PlatformEntry`（入口/主循环）与 `PlatformApp`
  （窗口/EGL/事件）分离，名字即意图

### 2.2 显式接口与命名

- 平台钩子函数成组命名（`TVPCreateTextureBackend`、
  `TVPSwapBuffersBackend`），语义一目了然，AI 可按名字推断契约
- 后端实现类名与用途一致（`GLRenderBackend` → `GLESRenderBackend`）
- 配置选项即文档：`KRKR_USE_EGL_GLES` / `KRKR_ANYGL_SWIFTSHADER`
  等选项名自解释

### 2.3 依赖显式化

- 依赖通过 manifest 声明（`vcpkg.json`），不依赖机器环境
- 头文件 include 最小化且顺序稳定（避免 Xlib 宏污染等隐式问题）
- 平台库（EGL/GLES/X11/Vulkan）与包管理器依赖边界清晰

### 2.4 构建系统可预测

- 选项化后端选择（CMake option），同一源码树可产出不同目标
- 支持差分编译（ninja 增量），单文件改动秒级验证
- 编译宏集中注入（见 `CMakeLists.txt` 注释分节），不散落各处

### 2.5 诊断友好

- 启动日志带关键决策点：`ANGLE display created with backend 'opengl'`
- 错误可复现：`KRKR_EGL_BACKEND`、`VK_ICD_FILENAMES` 等环境变量
  可固定/隔离变量做二分定位
- 失败路径有明确错误码与提示（EGL 错误码、Vulkan 扩展缺失等）

## 3. 文档层面的特点

- **架构说明**（README）：一张表讲清"什么场景走哪条路径"
- **构建文档**（BUILD.md）：命令可复制、选项有表格、含"不要做什么"
  （如不删构建目录）
- **调试文档**（DEBUGGING.md）：症状→原因→排查方向速查表，
  关键修复记录（含根因分析）
- **注释写意图不写过程**：`// Bind internal symbols to avoid zlib
  interposition` 比 `// add -Bsymbolic-functions` 有价值

## 4. 反面教训（本次踩坑总结）

| 坑 | 根因 | 对 AI 友好的写法 |
| --- | --- | --- |
| Vulkan 黑屏，shader 解压 -2 | 宿主可执行文件与动态库各自内嵌 zlib，符号插值混用 | 库内部符号 `-Bsymbolic-functions` 绑定；或在文档记录 ABI 边界 |
| `eglGetPlatformDisplayEXT` 一直 BAD_ATTRIBUTE | EXT 签名是 `EGLint*` 而标准版是 `EGLAttrib*`，混用导致属性错位 | 头文件/文档明确区分两个入口签名；调用处注释为何用 EXT |
| GCC 16 编译 ANGLE 失败 | 供应商代码与编译器版本不兼容 | 记录编译器要求（clang）；文档给出验证过的组合 |
| 误删构建目录导致全量重编 | ninja 增量状态依赖 `.ninja_log` | 文档显著位置写"不要删除构建目录" |
| 视频路径未启用 | `_KRKRSDL3_USE_FFMPEG` 只在一条分支定义 | 公共功能宏集中定义，不散落分支 |
| 文档路径含个人信息 | 硬编码了本机路径 | 用 `<vcpkg-root>`、`<game>` 占位符 |

## 5. 对 AI 辅助迁移的建议

1. **先文档后代码**：迁移前先产出"现状 + 目标 + 约束"文档，AI 按约束
   生成方案，人工校验边界。
2. **接口先行**：把引擎依赖的平台接口列成清单（本项目的
   `iTVPRenderBackend`/`iTVPSoundBuffer` 等），迁移就是逐个实现接口。
3. **小步验证**：每完成一个接口/模块即构建运行验证（差分编译保证
   反馈速度），失败立刻定位，避免大爆炸式改动。
4. **保留诊断手段**：运行日志、环境变量开关、最小复现程序
   （本项目用独立 egltest 隔离 ANGLE 问题）。
5. **沉淀经验**：每个根因分析写入调试文档，形成"症状速查表"，
   后续 AI 或人可按表快速定位。
