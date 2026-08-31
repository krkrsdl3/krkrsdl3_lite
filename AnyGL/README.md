# Outer ANGLE CMake build

This project builds ANGLE from the read-only `angle/` checkout using only this
root CMake project. No files under ANGLE or SwiftShader are modified.

## Backends

The Windows backend switches are independent:

```text
-Dangle_enable_vk=ON   # Vulkan renderer
-Dangle_enable_dx=ON   # D3D11 renderer
-Dangle_enable_gl=ON   # native WGL sample path
-Dangle_enable_cl=OFF  # OpenCL
```

Vulkan builds require the bundled VMA checkout and the SPIR-V headers/tools
available in `swiftshader/`. The outer build generates `vma_gen.cpp` and uses
`angle/third_party/vulkan_memory_allocator/include/vk_mem_alloc.h` as an input;
the vendor tree remains untouched.

## Build

```powershell
cmake -S . -B build-multi -G Ninja `
  -Dangle_enable_vk=ON -Dangle_enable_dx=ON -Dangle_enable_gl=ON `
  -DANGLE_SPIRV_TOOLS_ROOT=swiftshader/third_party/SPIRV-Tools
cmake --build build-multi --target angle_glesv2_shared angle_egl_shared angle_color_triangle_shared -- -j1
```

The core runtime artifacts are emitted under `build-multi/bin`:
`libEGL.dll`, `libGLESv2.dll`, `libGLESv1_CM.dll`, and the color triangle
example. `angle_enable_gl` on Windows means the sample's native WGL path; it is
not an ANGLE native-OpenGL renderer.

## Vulkan runtime selection

The default Vulkan loader path is **SYSTEM**. ANGLE loads the operating
system's `vulkan-1.dll`, so system/Android Vulkan is preferred, and Apple
systems should use the system Vulkan/MoltenVK installation. Do not rename
SwiftShader to `vulkan-1.dll` beside the application: that would shadow the
system loader.

`ANGLE_VULKAN_RUNTIME` accepts `SYSTEM`, `SWIFTSHADER`, or `AUTO` as a build
configuration marker. `BUILD_SWIFTSHADER_VULKAN=ON` builds SwiftShader outside
its checkout and stages `vk_swiftshader.dll` under the build tree's
`bin/swiftshader` directory. A forced SwiftShader run must select its ICD with
`VK_ICD_FILENAMES` and set `ANGLE_DEFAULT_PLATFORM=swiftshader`; an automatic
fallback must be implemented by an outer launcher that retries after system
Vulkan initialization fails.

## Tests

```powershell
$env:ANGLE_DEFAULT_PLATFORM='d3d11'; .\build-multi\bin\angle_color_triangle.exe
$env:ANGLE_DEFAULT_PLATFORM='vulkan'; .\build-multi\bin\angle_color_triangle.exe
$env:ANGLE_DEFAULT_PLATFORM=''; .\build-multi\bin\angle_color_triangle.exe --use-gl=native
```

The example prints EGL and GL vendor/renderer/version information, which makes
backend selection visible in CI and local tests.
