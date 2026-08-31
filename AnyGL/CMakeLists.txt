# ============================================================================
# AnyGL —— ANGLE/SwiftShader 的 CMake 构建编排
#
# 职责：将只读供应商源码（angle/、swiftshader/）编译为
#       libEGL.so / libangle_glesv2_shared.so，并可选构建 SwiftShader ICD。
# 约束：不得修改 angle/ 与 swiftshader/ 内任何文件；本文件与 AnyGL 顶层
#       新增文件（如 vma_wrapper.cpp）可自由维护。
# ============================================================================

###############
# 1. 核心路径（只读供应商目录）
###############
#   ANGLE_ROOT
#   CHROMIUM_ZLIB_ROOT
set(ANGLE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/angle)
set(CHROMIUM_ZLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/angle/third_party/zlib)
if(NOT EXISTS ${ANGLE_ROOT})
    message(FATAL_ERROR "ANGLE_ROOT is not found (${ANGLE_ROOT})")
endif()
if(NOT EXISTS ${CHROMIUM_ZLIB_ROOT})
    message(FATAL_ERROR "CHROMIUM_ZLIB is not found (${CHROMIUM_ZLIB_ROOT})")
endif()

###############
# 2. 建立项目
###############
cmake_minimum_required(VERSION 3.16)
project(ANGLE C CXX)
if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

###############
# 3. 配置（后端开关与运行参数）
###############
# 后端开关：angle_enable_* 决定编译进 ANGLE 的实现后端。
# 由上层（krkrlite 顶层 CMake 的 KRKR_ANYGL_* 选项）驱动；
# 多个后端可同时编译，运行时自动选择。
option(angle_enable_cl "Build OpenCL backend" OFF)
option(angle_enable_vk "Build Vulkan backend" ON)
option(angle_enable_dx "Build DirectX backends" ON)
option(angle_enable_gl "Build native OpenGL backend" ON)

# DirectX backends only exist on Windows; force them off elsewhere so the
# D3D sources are never added on Linux/macOS builds.
if(NOT WIN32)
    set(angle_enable_dx OFF CACHE BOOL "Build DirectX backends" FORCE)
endif()

# Backward-compatible selector: when explicitly set, enable that backend too.
if(DEFINED ANGLE_BACKEND)
    if(ANGLE_BACKEND STREQUAL "Vulkan")
        set(angle_enable_vk ON CACHE BOOL "Build Vulkan backend" FORCE)
    elseif(ANGLE_BACKEND STREQUAL "DirectX")
        set(angle_enable_dx ON CACHE BOOL "Build DirectX backends" FORCE)
    endif()
endif()

if(DEFINED KRKR_ANYGL_VULKAN AND KRKR_ANYGL_VULKAN)
    set(angle_enable_vk ON CACHE BOOL "Build Vulkan backend" FORCE)
    set(angle_enable_dx OFF CACHE BOOL "Build DirectX backends" FORCE)
endif()
if(DEFINED KRKR_ANYGL_OPENGL AND KRKR_ANYGL_OPENGL)
    set(angle_enable_gl ON CACHE BOOL "Build native OpenGL backend" FORCE)
endif()
if(DEFINED KRKR_ANYGL_SWIFTSHADER AND KRKR_ANYGL_SWIFTSHADER)
    set(BUILD_SWIFTSHADER_VULKAN ON CACHE BOOL "Build SwiftShader Vulkan ICD" FORCE)
    set(ANGLE_VULKAN_RUNTIME SWIFTSHADER CACHE STRING "Vulkan runtime selection" FORCE)
    # Point ANGLE at the generated SwiftShader ICD so the software renderer
    # is selected automatically at runtime (no VK_ICD_FILENAMES needed).
    # NOTE: ANGLE resolves this relative to the module directory of
    # libangle_glesv2_shared.so (build-anygl-sw/AnyGL/), so the path must be
    # relative; an absolute path gets concatenated and fails to load.
    add_definitions(-DANGLE_VK_SWIFTSHADER_ICD_JSON="../bin/swiftshader/vk_swiftshader_icd.json")
    # Activate the EGL_ANGLE_platform_angle_device_type_swiftshader client
    # extension; without it ANGLE rejects EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE
    # (EGL_BAD_ATTRIBUTE) and falls back to the hardware Vulkan driver.
    add_definitions(-DANGLE_ENABLE_SWIFTSHADER)
endif()

set(ANGLE_VMA_ROOT "${ANGLE_ROOT}/third_party/vulkan_memory_allocator" CACHE PATH "VMA checkout root")
set(ANGLE_VULKAN_RUNTIME "SYSTEM" CACHE STRING "Vulkan runtime selection: SYSTEM, SWIFTSHADER, or AUTO")
set_property(CACHE ANGLE_VULKAN_RUNTIME PROPERTY STRINGS SYSTEM SWIFTSHADER AUTO)
set(SWIFTSHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader" CACHE PATH "SwiftShader source directory")
set(SWIFTSHADER_DIST_DIR "${CMAKE_BINARY_DIR}/bin/swiftshader" CACHE PATH "SwiftShader runtime staging directory")

###############
# 4.源码
###############
# src/compiler.gni
set(angle_translator_exported_headers
    include/GLSLANG/ShaderLang.h
    include/GLSLANG/ShaderVars.h
    src/compiler/translator/blocklayout.h
    src/compiler/translator/hlsl/blocklayoutHLSL.h
)

set(angle_translator_sources
    include/EGL/egl.h
    include/EGL/eglext.h
    include/EGL/eglplatform.h
    include/GLES2/gl2.h
    include/GLES2/gl2ext.h
    include/GLES2/gl2platform.h
    include/GLES3/gl3.h
    include/GLES3/gl31.h
    include/GLES3/gl32.h
    include/GLES3/gl3platform.h
    include/KHR/khrplatform.h
    include/angle_gl.h
    src/compiler/translator/AsNode.h
    src/compiler/translator/BaseTypes.cpp
    src/compiler/translator/BaseTypes.h
    src/compiler/translator/BuiltInFunctionEmulator.cpp
    src/compiler/translator/BuiltInFunctionEmulator.h
    src/compiler/translator/CallDAG.cpp
    src/compiler/translator/CallDAG.h
    src/compiler/translator/CodeGen.cpp
    src/compiler/translator/CollectVariables.cpp
    src/compiler/translator/CollectVariables.h
    src/compiler/translator/Common.h
    src/compiler/translator/Compiler.cpp
    src/compiler/translator/Compiler.h
    src/compiler/translator/ConstantUnion.cpp
    src/compiler/translator/ConstantUnion.h
    src/compiler/translator/Declarator.cpp
    src/compiler/translator/Declarator.h
    src/compiler/translator/Diagnostics.cpp
    src/compiler/translator/Diagnostics.h
    src/compiler/translator/DirectiveHandler.cpp
    src/compiler/translator/DirectiveHandler.h
    src/compiler/translator/ExtensionBehavior.cpp
    src/compiler/translator/ExtensionBehavior.h
    src/compiler/translator/FlagStd140Structs.cpp
    src/compiler/translator/FlagStd140Structs.h
    src/compiler/translator/FunctionLookup.cpp
    src/compiler/translator/FunctionLookup.h
    src/compiler/translator/HashNames.cpp
    src/compiler/translator/HashNames.h
    src/compiler/translator/ImmutableString.h
    src/compiler/translator/ImmutableStringBuilder.cpp
    src/compiler/translator/ImmutableStringBuilder.h
    src/compiler/translator/InfoSink.cpp
    src/compiler/translator/InfoSink.h
    src/compiler/translator/Initialize.cpp
    src/compiler/translator/Initialize.h
    src/compiler/translator/InitializeGlobals.h
    src/compiler/translator/IntermNode.cpp
    src/compiler/translator/IntermNode.h
    src/compiler/translator/IntermRebuild.cpp
    src/compiler/translator/IntermRebuild.h
    src/compiler/translator/IsASTDepthBelowLimit.cpp
    src/compiler/translator/IsASTDepthBelowLimit.h
    src/compiler/translator/Name.cpp
    src/compiler/translator/Name.h
    src/compiler/translator/NodeType.h
    src/compiler/translator/Operator.cpp
    src/compiler/translator/Operator_autogen.h
    src/compiler/translator/OutputTree.cpp
    src/compiler/translator/OutputTree.h
    src/compiler/translator/ParseContext.cpp
    src/compiler/translator/ParseContext.h
    src/compiler/translator/PoolAlloc.cpp
    src/compiler/translator/PoolAlloc.h
    src/compiler/translator/Pragma.h
    src/compiler/translator/QualifierTypes.cpp
    src/compiler/translator/QualifierTypes.h
    src/compiler/translator/Severity.h
    src/compiler/translator/ShaderLang.cpp
    src/compiler/translator/ShaderVars.cpp
    src/compiler/translator/SizeClipCullDistance.cpp
    src/compiler/translator/SizeClipCullDistance.h
    src/compiler/translator/StaticType.h
    src/compiler/translator/Symbol.cpp
    src/compiler/translator/Symbol.h
    src/compiler/translator/SymbolTable.cpp
    src/compiler/translator/SymbolTable.h
    src/compiler/translator/SymbolTable_autogen.h
    src/compiler/translator/SymbolUniqueId.cpp
    src/compiler/translator/SymbolUniqueId.h
    src/compiler/translator/Types.cpp
    src/compiler/translator/Types.h
    src/compiler/translator/ValidateAST.cpp
    src/compiler/translator/ValidateAST.h
    src/compiler/translator/ValidateGlobalInitializer.cpp
    src/compiler/translator/ValidateGlobalInitializer.h
    src/compiler/translator/ValidateVaryingLocations.cpp
    src/compiler/translator/ValidateVaryingLocations.h
    src/compiler/translator/VariablePacker.cpp
    src/compiler/translator/VariablePacker.h
    src/compiler/translator/blocklayout.cpp
    src/compiler/translator/glsl/TranslatorESSL.h
    src/compiler/translator/glsl/TranslatorGLSL.h
    src/compiler/translator/glsl/VersionGLSL.h
    src/compiler/translator/glslang.h
    src/compiler/translator/glslang_lex_autogen.cpp
    src/compiler/translator/glslang_tab_autogen.cpp
    src/compiler/translator/glslang_tab_autogen.h
    src/compiler/translator/hlsl/TranslatorHLSL.h
    src/compiler/translator/ir/src/builder.h
    src/compiler/translator/ir/src/compile.h
    src/compiler/translator/ir/src/output/ir_to_legacy.h
    src/compiler/translator/ir/src/output/legacy.h
    src/compiler/translator/ir/src/pool_alloc.h
    src/compiler/translator/length_limits.h
    src/compiler/translator/msl/DriverUniformMetal.h
    src/compiler/translator/msl/TranslatorMSL.h
    src/compiler/translator/null/TranslatorNULL.h
    src/compiler/translator/spirv/TranslatorSPIRV.h
    src/compiler/translator/tree_ops/AddDefaultReturnStatements.cpp
    src/compiler/translator/tree_ops/AddDefaultReturnStatements.h
    src/compiler/translator/tree_ops/ClampFragDepth.cpp
    src/compiler/translator/tree_ops/ClampFragDepth.h
    src/compiler/translator/tree_ops/ClampIndirectIndices.cpp
    src/compiler/translator/tree_ops/ClampIndirectIndices.h
    src/compiler/translator/tree_ops/ClampPointSize.cpp
    src/compiler/translator/tree_ops/ClampPointSize.h
    src/compiler/translator/tree_ops/DeclareAndInitBuiltinsForInstancedMultiview.cpp
    src/compiler/translator/tree_ops/DeclareAndInitBuiltinsForInstancedMultiview.h
    src/compiler/translator/tree_ops/DeclarePerVertexBlocks.cpp
    src/compiler/translator/tree_ops/DeclarePerVertexBlocks.h
    src/compiler/translator/tree_ops/DeferGlobalInitializers.cpp
    src/compiler/translator/tree_ops/DeferGlobalInitializers.h
    src/compiler/translator/tree_ops/EmulateGLFragColorBroadcast.cpp
    src/compiler/translator/tree_ops/EmulateGLFragColorBroadcast.h
    src/compiler/translator/tree_ops/EmulateMultiDrawShaderBuiltins.cpp
    src/compiler/translator/tree_ops/EmulateMultiDrawShaderBuiltins.h
    src/compiler/translator/tree_ops/FoldExpressions.cpp
    src/compiler/translator/tree_ops/FoldExpressions.h
    src/compiler/translator/tree_ops/GatherDefaultUniforms.cpp
    src/compiler/translator/tree_ops/GatherDefaultUniforms.h
    src/compiler/translator/tree_ops/InitializeVariables.cpp
    src/compiler/translator/tree_ops/InitializeVariables.h
    src/compiler/translator/tree_ops/MonomorphizeUnsupportedFunctions.cpp
    src/compiler/translator/tree_ops/MonomorphizeUnsupportedFunctions.h
    src/compiler/translator/tree_ops/PreTransformTextureCubeGradDerivatives.cpp
    src/compiler/translator/tree_ops/PreTransformTextureCubeGradDerivatives.h
    src/compiler/translator/tree_ops/PruneEmptyCases.cpp
    src/compiler/translator/tree_ops/PruneEmptyCases.h
    src/compiler/translator/tree_ops/PruneNoOps.cpp
    src/compiler/translator/tree_ops/PruneNoOps.h
    src/compiler/translator/tree_ops/RecordConstantPrecision.cpp
    src/compiler/translator/tree_ops/RecordConstantPrecision.h
    src/compiler/translator/tree_ops/ReduceInterfaceBlocks.cpp
    src/compiler/translator/tree_ops/ReduceInterfaceBlocks.h
    src/compiler/translator/tree_ops/RemoveArrayLengthMethod.cpp
    src/compiler/translator/tree_ops/RemoveArrayLengthMethod.h
    src/compiler/translator/tree_ops/RemoveAtomicCounterBuiltins.cpp
    src/compiler/translator/tree_ops/RemoveAtomicCounterBuiltins.h
    src/compiler/translator/tree_ops/RemoveDynamicIndexing.cpp
    src/compiler/translator/tree_ops/RemoveDynamicIndexing.h
    src/compiler/translator/tree_ops/RemoveInactiveInterfaceVariables.cpp
    src/compiler/translator/tree_ops/RemoveInactiveInterfaceVariables.h
    src/compiler/translator/tree_ops/RemoveInvariantDeclaration.cpp
    src/compiler/translator/tree_ops/RemoveInvariantDeclaration.h
    src/compiler/translator/tree_ops/RemoveUnreferencedVariables.cpp
    src/compiler/translator/tree_ops/RemoveUnreferencedVariables.h
    src/compiler/translator/tree_ops/RemoveUnusedFramebufferFetch.cpp
    src/compiler/translator/tree_ops/RemoveUnusedFramebufferFetch.h
    src/compiler/translator/tree_ops/RewriteArrayOfArrayOfOpaqueUniforms.cpp
    src/compiler/translator/tree_ops/RewriteArrayOfArrayOfOpaqueUniforms.h
    src/compiler/translator/tree_ops/RewriteAtomicCounters.cpp
    src/compiler/translator/tree_ops/RewriteAtomicCounters.h
    src/compiler/translator/tree_ops/RewriteDfdy.cpp
    src/compiler/translator/tree_ops/RewriteDfdy.h
    src/compiler/translator/tree_ops/RewritePixelLocalStorage.cpp
    src/compiler/translator/tree_ops/RewritePixelLocalStorage.h
    src/compiler/translator/tree_ops/RewriteStructSamplers.cpp
    src/compiler/translator/tree_ops/RewriteStructSamplers.h
    src/compiler/translator/tree_ops/RewriteTexelFetchOffset.cpp
    src/compiler/translator/tree_ops/RewriteTexelFetchOffset.h
    src/compiler/translator/tree_ops/ScalarizeVecAndMatConstructorArgs.cpp
    src/compiler/translator/tree_ops/ScalarizeVecAndMatConstructorArgs.h
    src/compiler/translator/tree_ops/SeparateDeclarations.cpp
    src/compiler/translator/tree_ops/SeparateDeclarations.h
    src/compiler/translator/tree_ops/SeparateStructFromUniformDeclarations.cpp
    src/compiler/translator/tree_ops/SeparateStructFromUniformDeclarations.h
    src/compiler/translator/tree_ops/SimplifyLoopConditions.cpp
    src/compiler/translator/tree_ops/SimplifyLoopConditions.h
    src/compiler/translator/tree_ops/SplitSequenceOperator.cpp
    src/compiler/translator/tree_ops/SplitSequenceOperator.h
    src/compiler/translator/tree_ops/glsl/ExpandFragmentOutputsToVec4.h
    src/compiler/translator/tree_ops/glsl/RewriteRepeatedAssignToSwizzled.h
    src/compiler/translator/tree_ops/glsl/UseInterfaceBlockFields.h
    src/compiler/translator/tree_ops/glsl/WrapStructConstructors.h
    src/compiler/translator/tree_ops/glsl/apple/AddAndTrueToLoopCondition.h
    src/compiler/translator/tree_ops/glsl/apple/RewriteRowMajorMatrices.h
    src/compiler/translator/tree_ops/glsl/apple/UnfoldShortCircuitAST.h
    src/compiler/translator/tree_ops/msl/EnsureLoopForwardProgress.h
    src/compiler/translator/tree_util/BuiltIn.h
    src/compiler/translator/tree_util/BuiltIn_autogen.h
    src/compiler/translator/tree_util/DriverUniform.cpp
    src/compiler/translator/tree_util/DriverUniform.h
    src/compiler/translator/tree_util/FindFunction.cpp
    src/compiler/translator/tree_util/FindFunction.h
    src/compiler/translator/tree_util/FindMain.cpp
    src/compiler/translator/tree_util/FindMain.h
    src/compiler/translator/tree_util/FindPreciseNodes.cpp
    src/compiler/translator/tree_util/FindPreciseNodes.h
    src/compiler/translator/tree_util/FindSymbolNode.cpp
    src/compiler/translator/tree_util/FindSymbolNode.h
    src/compiler/translator/tree_util/IntermNodePatternMatcher.cpp
    src/compiler/translator/tree_util/IntermNodePatternMatcher.h
    src/compiler/translator/tree_util/IntermNode_util.cpp
    src/compiler/translator/tree_util/IntermNode_util.h
    src/compiler/translator/tree_util/IntermTraverse.cpp
    src/compiler/translator/tree_util/IntermTraverse.h
    src/compiler/translator/tree_util/NodeSearch.h
    src/compiler/translator/tree_util/ReplaceArrayOfMatrixVarying.cpp
    src/compiler/translator/tree_util/ReplaceArrayOfMatrixVarying.h
    src/compiler/translator/tree_util/ReplaceClipCullDistanceVariable.cpp
    src/compiler/translator/tree_util/ReplaceClipCullDistanceVariable.h
    src/compiler/translator/tree_util/ReplaceShadowingVariables.cpp
    src/compiler/translator/tree_util/ReplaceShadowingVariables.h
    src/compiler/translator/tree_util/ReplaceVariable.cpp
    src/compiler/translator/tree_util/ReplaceVariable.h
    src/compiler/translator/tree_util/RewriteSampleMaskVariable.cpp
    src/compiler/translator/tree_util/RewriteSampleMaskVariable.h
    src/compiler/translator/tree_util/RunAtTheBeginningOfShader.cpp
    src/compiler/translator/tree_util/RunAtTheBeginningOfShader.h
    src/compiler/translator/tree_util/RunAtTheEndOfShader.cpp
    src/compiler/translator/tree_util/RunAtTheEndOfShader.h
    src/compiler/translator/tree_util/Visit.h
    src/compiler/translator/util.cpp
    src/compiler/translator/util.h
    src/compiler/translator/wgsl/TranslatorWGSL.h
)

set(angle_translator_glsl_base_sources
    src/compiler/translator/glsl/OutputGLSLBase.cpp
    src/compiler/translator/glsl/OutputGLSLBase.h
)

set(angle_translator_glsl_and_vulkan_base_sources
    src/compiler/translator/glsl/OutputGLSL.cpp
    src/compiler/translator/glsl/OutputGLSL.h
)

set(angle_translator_essl_sources
    src/compiler/translator/glsl/OutputESSL.cpp
    src/compiler/translator/glsl/OutputESSL.h
    src/compiler/translator/glsl/TranslatorESSL.cpp
)

set(angle_translator_glsl_sources
    src/compiler/translator/glsl/BuiltInFunctionEmulatorGLSL.cpp
    src/compiler/translator/glsl/BuiltInFunctionEmulatorGLSL.h
    src/compiler/translator/glsl/ExtensionGLSL.cpp
    src/compiler/translator/glsl/ExtensionGLSL.h
    src/compiler/translator/glsl/TranslatorGLSL.cpp
    src/compiler/translator/glsl/VersionGLSL.cpp
    src/compiler/translator/tree_ops/glsl/RegenerateStructNames.cpp
    src/compiler/translator/tree_ops/glsl/RewriteRepeatedAssignToSwizzled.cpp
    src/compiler/translator/tree_ops/glsl/ScalarizeVecAndMatConstructorArgs.cpp
    src/compiler/translator/tree_ops/glsl/UseInterfaceBlockFields.cpp
)

set(angle_translator_glsl_apple_sources
    src/compiler/translator/tree_ops/glsl/apple/AddAndTrueToLoopCondition.cpp
    src/compiler/translator/tree_ops/glsl/apple/RewriteRowMajorMatrices.cpp
    src/compiler/translator/tree_ops/glsl/apple/UnfoldShortCircuitAST.cpp
)

set(angle_translator_hlsl_sources
    src/compiler/translator/hlsl/ASTMetadataHLSL.cpp
    src/compiler/translator/hlsl/ASTMetadataHLSL.h
    src/compiler/translator/hlsl/BuiltInFunctionEmulatorHLSL.cpp
    src/compiler/translator/hlsl/BuiltInFunctionEmulatorHLSL.h
    src/compiler/translator/hlsl/ImageFunctionHLSL.cpp
    src/compiler/translator/hlsl/ImageFunctionHLSL.h
    src/compiler/translator/hlsl/OutputHLSL.cpp
    src/compiler/translator/hlsl/OutputHLSL.h
    src/compiler/translator/hlsl/ResourcesHLSL.cpp
    src/compiler/translator/hlsl/ResourcesHLSL.h
    src/compiler/translator/hlsl/StructureHLSL.cpp
    src/compiler/translator/hlsl/StructureHLSL.h
    src/compiler/translator/hlsl/TextureFunctionHLSL.cpp
    src/compiler/translator/hlsl/TextureFunctionHLSL.h
    src/compiler/translator/hlsl/TranslatorHLSL.cpp
    src/compiler/translator/hlsl/UtilsHLSL.cpp
    src/compiler/translator/hlsl/UtilsHLSL.h
    src/compiler/translator/hlsl/blocklayoutHLSL.cpp
    src/compiler/translator/hlsl/emulated_builtin_functions_hlsl_autogen.cpp
    src/compiler/translator/tree_ops/hlsl/ArrayReturnValueToOutParameter.cpp
    src/compiler/translator/tree_ops/hlsl/ArrayReturnValueToOutParameter.h
    src/compiler/translator/tree_ops/hlsl/BreakVariableAliasingInInnerLoops.cpp
    src/compiler/translator/tree_ops/hlsl/BreakVariableAliasingInInnerLoops.h
    src/compiler/translator/tree_ops/hlsl/ExpandIntegerPowExpressions.cpp
    src/compiler/translator/tree_ops/hlsl/ExpandIntegerPowExpressions.h
    src/compiler/translator/tree_ops/hlsl/RecordUniformBlocksWithLargeArrayMember.cpp
    src/compiler/translator/tree_ops/hlsl/RecordUniformBlocksWithLargeArrayMember.h
    src/compiler/translator/tree_ops/hlsl/RemoveSwitchFallThrough.cpp
    src/compiler/translator/tree_ops/hlsl/RemoveSwitchFallThrough.h
    src/compiler/translator/tree_ops/hlsl/RewriteElseBlocks.cpp
    src/compiler/translator/tree_ops/hlsl/RewriteElseBlocks.h
    src/compiler/translator/tree_ops/hlsl/RewriteUnaryMinusOperatorInt.cpp
    src/compiler/translator/tree_ops/hlsl/RewriteUnaryMinusOperatorInt.h
    src/compiler/translator/tree_ops/hlsl/SeparateArrayConstructorStatements.cpp
    src/compiler/translator/tree_ops/hlsl/SeparateArrayConstructorStatements.h
    src/compiler/translator/tree_ops/hlsl/SeparateArrayInitialization.cpp
    src/compiler/translator/tree_ops/hlsl/SeparateArrayInitialization.h
    src/compiler/translator/tree_ops/hlsl/SeparateExpressionsReturningArrays.cpp
    src/compiler/translator/tree_ops/hlsl/SeparateExpressionsReturningArrays.h
    src/compiler/translator/tree_ops/hlsl/UnfoldShortCircuitToIf.cpp
    src/compiler/translator/tree_ops/hlsl/UnfoldShortCircuitToIf.h
    src/compiler/translator/tree_ops/hlsl/WrapSwitchStatementsInBlocks.cpp
    src/compiler/translator/tree_ops/hlsl/WrapSwitchStatementsInBlocks.h
)

set(angle_translator_lib_spirv_sources
    src/compiler/translator/spirv/BuildSPIRV.cpp
    src/compiler/translator/spirv/BuildSPIRV.h
    src/compiler/translator/spirv/BuiltinsWorkaround.cpp
    src/compiler/translator/spirv/BuiltinsWorkaround.h
    src/compiler/translator/spirv/OutputSPIRV.cpp
    src/compiler/translator/spirv/OutputSPIRV.h
    src/compiler/translator/spirv/TranslatorSPIRV.cpp
    src/compiler/translator/tree_ops/spirv/ClampGLLayer.cpp
    src/compiler/translator/tree_ops/spirv/ClampGLLayer.h
    src/compiler/translator/tree_ops/spirv/EmulateAdvancedBlendEquations.cpp
    src/compiler/translator/tree_ops/spirv/EmulateAdvancedBlendEquations.h
    src/compiler/translator/tree_ops/spirv/EmulateFragColorData.cpp
    src/compiler/translator/tree_ops/spirv/EmulateFragColorData.h
    src/compiler/translator/tree_ops/spirv/EmulateFramebufferFetch.cpp
    src/compiler/translator/tree_ops/spirv/EmulateFramebufferFetch.h
    src/compiler/translator/tree_ops/spirv/EmulateYUVBuiltIns.cpp
    src/compiler/translator/tree_ops/spirv/EmulateYUVBuiltIns.h
    src/compiler/translator/tree_ops/spirv/FlagSamplersWithTexelFetch.cpp
    src/compiler/translator/tree_ops/spirv/FlagSamplersWithTexelFetch.h
    src/compiler/translator/tree_ops/spirv/ReswizzleYUVOps.cpp
    src/compiler/translator/tree_ops/spirv/ReswizzleYUVOps.h
    src/compiler/translator/tree_ops/spirv/RewriteInterpolateAtOffset.cpp
    src/compiler/translator/tree_ops/spirv/RewriteInterpolateAtOffset.h
    src/compiler/translator/tree_ops/spirv/RewriteR32fImages.cpp
    src/compiler/translator/tree_ops/spirv/RewriteR32fImages.h
    src/compiler/translator/tree_ops/spirv/RewriteSamplerExternalTexelFetch.cpp
    src/compiler/translator/tree_ops/spirv/RewriteSamplerExternalTexelFetch.h
)

set(angle_translator_essl_symbol_table_sources
    src/compiler/translator/ImmutableString_autogen.cpp
    src/compiler/translator/SymbolTable_autogen.cpp
)

set(angle_translator_lib_msl_sources
    src/compiler/translator/msl/AstHelpers.cpp
    src/compiler/translator/msl/AstHelpers.h
    src/compiler/translator/msl/ConstantNames.cpp
    src/compiler/translator/msl/DebugSink.h
    src/compiler/translator/msl/DiscoverDependentFunctions.cpp
    src/compiler/translator/msl/DiscoverDependentFunctions.h
    src/compiler/translator/msl/DiscoverEnclosingFunctionTraverser.cpp
    src/compiler/translator/msl/DiscoverEnclosingFunctionTraverser.h
    src/compiler/translator/msl/DriverUniformMetal.cpp
    src/compiler/translator/msl/EmitMetal.cpp
    src/compiler/translator/msl/EmitMetal.h
    src/compiler/translator/msl/IdGen.cpp
    src/compiler/translator/msl/IdGen.h
    src/compiler/translator/msl/Layout.cpp
    src/compiler/translator/msl/Layout.h
    src/compiler/translator/msl/MapFunctionsToDefinitions.cpp
    src/compiler/translator/msl/MapFunctionsToDefinitions.h
    src/compiler/translator/msl/MapSymbols.cpp
    src/compiler/translator/msl/MapSymbols.h
    src/compiler/translator/msl/ModifyStruct.cpp
    src/compiler/translator/msl/ModifyStruct.h
    src/compiler/translator/msl/Pipeline.cpp
    src/compiler/translator/msl/Pipeline.h
    src/compiler/translator/msl/ProgramPrelude.cpp
    src/compiler/translator/msl/ProgramPrelude.h
    src/compiler/translator/msl/Reference.h
    src/compiler/translator/msl/RewritePipelines.cpp
    src/compiler/translator/msl/RewritePipelines.h
    src/compiler/translator/msl/SkippingTraverser.h
    src/compiler/translator/msl/SymbolEnv.cpp
    src/compiler/translator/msl/SymbolEnv.h
    src/compiler/translator/msl/ToposortStructs.cpp
    src/compiler/translator/msl/ToposortStructs.h
    src/compiler/translator/msl/TranslatorMSL.cpp
    src/compiler/translator/msl/UtilsMSL.cpp
    src/compiler/translator/msl/UtilsMSL.h
    src/compiler/translator/tree_ops/msl/AddExplicitTypeCasts.cpp
    src/compiler/translator/tree_ops/msl/AddExplicitTypeCasts.h
    src/compiler/translator/tree_ops/msl/ConvertUnsupportedConstructorsToFunctionCalls.cpp
    src/compiler/translator/tree_ops/msl/ConvertUnsupportedConstructorsToFunctionCalls.h
    src/compiler/translator/tree_ops/msl/EnsureLoopForwardProgress.cpp
    src/compiler/translator/tree_ops/msl/FixTypeConstructors.cpp
    src/compiler/translator/tree_ops/msl/FixTypeConstructors.h
    src/compiler/translator/tree_ops/msl/GuardFragDepthWrite.cpp
    src/compiler/translator/tree_ops/msl/GuardFragDepthWrite.h
    src/compiler/translator/tree_ops/msl/HoistConstants.cpp
    src/compiler/translator/tree_ops/msl/HoistConstants.h
    src/compiler/translator/tree_ops/msl/IntroduceVertexIndexID.cpp
    src/compiler/translator/tree_ops/msl/IntroduceVertexIndexID.h
    src/compiler/translator/tree_ops/msl/RescopeGlobalVariables.cpp
    src/compiler/translator/tree_ops/msl/RescopeGlobalVariables.h
    src/compiler/translator/tree_ops/msl/RewriteCaseDeclarations.cpp
    src/compiler/translator/tree_ops/msl/RewriteCaseDeclarations.h
    src/compiler/translator/tree_ops/msl/RewriteInterpolants.cpp
    src/compiler/translator/tree_ops/msl/RewriteInterpolants.h
    src/compiler/translator/tree_ops/msl/RewriteOutArgs.cpp
    src/compiler/translator/tree_ops/msl/RewriteOutArgs.h
    src/compiler/translator/tree_ops/msl/RewriteUnaddressableReferences.cpp
    src/compiler/translator/tree_ops/msl/RewriteUnaddressableReferences.h
    src/compiler/translator/tree_ops/msl/SeparateCompoundExpressions.cpp
    src/compiler/translator/tree_ops/msl/SeparateCompoundExpressions.h
    src/compiler/translator/tree_ops/msl/TransposeRowMajorMatrices.cpp
    src/compiler/translator/tree_ops/msl/TransposeRowMajorMatrices.h
    src/compiler/translator/tree_ops/msl/WrapMain.cpp
    src/compiler/translator/tree_ops/msl/WrapMain.h
)

set(angle_translator_lib_wgsl_sources
    src/compiler/translator/tree_ops/wgsl/EmulateMutableFunctionParams.cpp
    src/compiler/translator/tree_ops/wgsl/EmulateMutableFunctionParams.h
    src/compiler/translator/tree_ops/wgsl/PullExpressionsIntoFunctions.cpp
    src/compiler/translator/tree_ops/wgsl/PullExpressionsIntoFunctions.h
    src/compiler/translator/tree_ops/wgsl/RewriteMixedTypeMathExprs.cpp
    src/compiler/translator/tree_ops/wgsl/RewriteMixedTypeMathExprs.h
    src/compiler/translator/tree_ops/wgsl/RewriteMultielementSwizzleAssignment.cpp
    src/compiler/translator/tree_ops/wgsl/RewriteMultielementSwizzleAssignment.h
    src/compiler/translator/wgsl/OutputUniformBlocks.cpp
    src/compiler/translator/wgsl/OutputUniformBlocks.h
    src/compiler/translator/wgsl/RewritePipelineVariables.cpp
    src/compiler/translator/wgsl/RewritePipelineVariables.h
    src/compiler/translator/wgsl/TranslatorWGSL.cpp
    src/compiler/translator/wgsl/Utils.cpp
    src/compiler/translator/wgsl/Utils.h
    src/compiler/translator/wgsl/WGSLProgramPrelude.cpp
    src/compiler/translator/wgsl/WGSLProgramPrelude.h
)

set(angle_translator_lib_ir_sources
    src/compiler/translator/ir/src/builder.cpp
    src/compiler/translator/ir/src/compile.cpp
    src/compiler/translator/ir/src/output/ir_to_legacy.cpp
    src/compiler/translator/ir/src/pool_alloc.cpp
)

set(angle_preprocessor_sources
    src/compiler/preprocessor/DiagnosticsBase.cpp
    src/compiler/preprocessor/DiagnosticsBase.h
    src/compiler/preprocessor/DirectiveHandlerBase.cpp
    src/compiler/preprocessor/DirectiveHandlerBase.h
    src/compiler/preprocessor/DirectiveParser.cpp
    src/compiler/preprocessor/DirectiveParser.h
    src/compiler/preprocessor/ExpressionParser.h
    src/compiler/preprocessor/Input.cpp
    src/compiler/preprocessor/Input.h
    src/compiler/preprocessor/Lexer.cpp
    src/compiler/preprocessor/Lexer.h
    src/compiler/preprocessor/Macro.cpp
    src/compiler/preprocessor/Macro.h
    src/compiler/preprocessor/MacroExpander.cpp
    src/compiler/preprocessor/MacroExpander.h
    src/compiler/preprocessor/Preprocessor.cpp
    src/compiler/preprocessor/Preprocessor.h
    src/compiler/preprocessor/SourceLocation.h
    src/compiler/preprocessor/Token.cpp
    src/compiler/preprocessor/Token.h
    src/compiler/preprocessor/Tokenizer.h
    src/compiler/preprocessor/numeric_lex.h
    src/compiler/preprocessor/preprocessor_lex_autogen.cpp
    src/compiler/preprocessor/preprocessor_tab_autogen.cpp
)

# src/libANGLE/renderer/vulkan/vulkan_backend.gni

set(angle_vulkan_backend_sources
    src/libANGLE/renderer/vulkan/AllocatorHelperPool.cpp
    src/libANGLE/renderer/vulkan/AllocatorHelperPool.h
    src/libANGLE/renderer/vulkan/BufferVk.cpp
    src/libANGLE/renderer/vulkan/BufferVk.h
    src/libANGLE/renderer/vulkan/CommandQueue.cpp
    src/libANGLE/renderer/vulkan/CommandQueue.h
    src/libANGLE/renderer/vulkan/CompilerVk.cpp
    src/libANGLE/renderer/vulkan/CompilerVk.h
    src/libANGLE/renderer/vulkan/ContextVk.cpp
    src/libANGLE/renderer/vulkan/ContextVk.h
    src/libANGLE/renderer/vulkan/DebugAnnotatorVk.cpp
    src/libANGLE/renderer/vulkan/DebugAnnotatorVk.h
    src/libANGLE/renderer/vulkan/DeviceVk.cpp
    src/libANGLE/renderer/vulkan/DeviceVk.h
    src/libANGLE/renderer/vulkan/DisplayVk.cpp
    src/libANGLE/renderer/vulkan/DisplayVk.h
    src/libANGLE/renderer/vulkan/DisplayVk_api.h
    src/libANGLE/renderer/vulkan/DriverUniforms.h
    src/libANGLE/renderer/vulkan/FenceNVVk.cpp
    src/libANGLE/renderer/vulkan/FenceNVVk.h
    src/libANGLE/renderer/vulkan/FramebufferVk.cpp
    src/libANGLE/renderer/vulkan/FramebufferVk.h
    src/libANGLE/renderer/vulkan/ImageVk.cpp
    src/libANGLE/renderer/vulkan/ImageVk.h
    src/libANGLE/renderer/vulkan/MemoryObjectVk.cpp
    src/libANGLE/renderer/vulkan/MemoryObjectVk.h
    src/libANGLE/renderer/vulkan/MemoryTracking.cpp
    src/libANGLE/renderer/vulkan/MemoryTracking.h
    src/libANGLE/renderer/vulkan/PersistentCommandPool.cpp
    src/libANGLE/renderer/vulkan/PersistentCommandPool.h
    src/libANGLE/renderer/vulkan/ProgramExecutableVk.cpp
    src/libANGLE/renderer/vulkan/ProgramExecutableVk.h
    src/libANGLE/renderer/vulkan/ProgramPipelineVk.cpp
    src/libANGLE/renderer/vulkan/ProgramPipelineVk.h
    src/libANGLE/renderer/vulkan/ProgramVk.cpp
    src/libANGLE/renderer/vulkan/ProgramVk.h
    src/libANGLE/renderer/vulkan/QueryVk.cpp
    src/libANGLE/renderer/vulkan/QueryVk.h
    src/libANGLE/renderer/vulkan/RenderTargetVk.cpp
    src/libANGLE/renderer/vulkan/RenderTargetVk.h
    src/libANGLE/renderer/vulkan/RenderbufferVk.cpp
    src/libANGLE/renderer/vulkan/RenderbufferVk.h
    src/libANGLE/renderer/vulkan/SamplerVk.cpp
    src/libANGLE/renderer/vulkan/SamplerVk.h
    src/libANGLE/renderer/vulkan/SecondaryCommandBuffer.cpp
    src/libANGLE/renderer/vulkan/SecondaryCommandBuffer.h
    src/libANGLE/renderer/vulkan/SecondaryCommandPool.cpp
    src/libANGLE/renderer/vulkan/SecondaryCommandPool.h
    src/libANGLE/renderer/vulkan/SemaphoreVk.cpp
    src/libANGLE/renderer/vulkan/SemaphoreVk.h
    src/libANGLE/renderer/vulkan/ShaderInterfaceVariableInfoMap.cpp
    src/libANGLE/renderer/vulkan/ShaderInterfaceVariableInfoMap.h
    src/libANGLE/renderer/vulkan/ShaderVk.cpp
    src/libANGLE/renderer/vulkan/ShaderVk.h
    src/libANGLE/renderer/vulkan/ShareGroupVk.cpp
    src/libANGLE/renderer/vulkan/ShareGroupVk.h
    src/libANGLE/renderer/vulkan/Suballocation.cpp
    src/libANGLE/renderer/vulkan/Suballocation.h
    src/libANGLE/renderer/vulkan/SurfaceVk.cpp
    src/libANGLE/renderer/vulkan/SurfaceVk.h
    src/libANGLE/renderer/vulkan/SyncVk.cpp
    src/libANGLE/renderer/vulkan/SyncVk.h
    src/libANGLE/renderer/vulkan/TextureVk.cpp
    src/libANGLE/renderer/vulkan/TextureVk.h
    src/libANGLE/renderer/vulkan/TransformFeedbackVk.cpp
    src/libANGLE/renderer/vulkan/TransformFeedbackVk.h
    src/libANGLE/renderer/vulkan/UtilsVk.cpp
    src/libANGLE/renderer/vulkan/UtilsVk.h
    src/libANGLE/renderer/vulkan/VertexArrayVk.cpp
    src/libANGLE/renderer/vulkan/VertexArrayVk.h
    src/libANGLE/renderer/vulkan/VkImageImageSiblingVk.cpp
    src/libANGLE/renderer/vulkan/VkImageImageSiblingVk.h
    src/libANGLE/renderer/vulkan/VulkanSecondaryCommandBuffer.cpp
    src/libANGLE/renderer/vulkan/VulkanSecondaryCommandBuffer.h
    src/libANGLE/renderer/vulkan/android/vk_android_utils.cpp
    src/libANGLE/renderer/vulkan/android/vk_android_utils.h
    src/libANGLE/renderer/vulkan/spv_utils.cpp
    src/libANGLE/renderer/vulkan/spv_utils.h
    src/libANGLE/renderer/vulkan/vk_api_perf_counters.cpp
    src/libANGLE/renderer/vulkan/vk_api_perf_counters.h
    src/libANGLE/renderer/vulkan/vk_barrier_data.cpp
    src/libANGLE/renderer/vulkan/vk_barrier_data.h
    src/libANGLE/renderer/vulkan/vk_cache_utils.cpp
    src/libANGLE/renderer/vulkan/vk_cache_utils.h
    src/libANGLE/renderer/vulkan/vk_caps_utils.cpp
    src/libANGLE/renderer/vulkan/vk_caps_utils.h
    src/libANGLE/renderer/vulkan/vk_command_buffer_utils.h
    src/libANGLE/renderer/vulkan/vk_format_table_autogen.cpp
    src/libANGLE/renderer/vulkan/vk_format_utils.cpp
    src/libANGLE/renderer/vulkan/vk_format_utils.h
    src/libANGLE/renderer/vulkan/vk_helpers.cpp
    src/libANGLE/renderer/vulkan/vk_helpers.h
    src/libANGLE/renderer/vulkan/vk_internal_shaders_autogen.cpp
    src/libANGLE/renderer/vulkan/vk_internal_shaders_autogen.h
    src/libANGLE/renderer/vulkan/vk_mandatory_format_support_table_autogen.cpp
    src/libANGLE/renderer/vulkan/vk_ref_counted_event.cpp
    src/libANGLE/renderer/vulkan/vk_ref_counted_event.h
    src/libANGLE/renderer/vulkan/vk_renderer.cpp
    src/libANGLE/renderer/vulkan/vk_renderer.h
    src/libANGLE/renderer/vulkan/vk_resource.cpp
    src/libANGLE/renderer/vulkan/vk_resource.h
    src/libANGLE/renderer/vulkan/vk_utils.cpp
    src/libANGLE/renderer/vulkan/vk_utils.h
    src/libANGLE/renderer/vulkan/vk_wrapper.h
)

if(angle_enable_cl)
    list(APPEND angle_vulkan_backend_sources
        src/libANGLE/renderer/vulkan/CLCommandQueueVk.cpp
        src/libANGLE/renderer/vulkan/CLCommandQueueVk.h
        src/libANGLE/renderer/vulkan/CLContextVk.cpp
        src/libANGLE/renderer/vulkan/CLContextVk.h
        src/libANGLE/renderer/vulkan/CLDeviceVk.cpp
        src/libANGLE/renderer/vulkan/CLDeviceVk.h
        src/libANGLE/renderer/vulkan/CLEventVk.cpp
        src/libANGLE/renderer/vulkan/CLEventVk.h
        src/libANGLE/renderer/vulkan/CLKernelVk.cpp
        src/libANGLE/renderer/vulkan/CLKernelVk.h
        src/libANGLE/renderer/vulkan/CLMemoryVk.cpp
        src/libANGLE/renderer/vulkan/CLMemoryVk.h
        src/libANGLE/renderer/vulkan/CLPlatformVk.cpp
        src/libANGLE/renderer/vulkan/CLPlatformVk.h
        src/libANGLE/renderer/vulkan/CLProgramVk.cpp
        src/libANGLE/renderer/vulkan/CLProgramVk.h
        src/libANGLE/renderer/vulkan/CLSamplerVk.cpp
        src/libANGLE/renderer/vulkan/CLSamplerVk.h
        src/libANGLE/renderer/vulkan/cl_types.h
        src/libANGLE/renderer/vulkan/clspv_utils.cpp
        src/libANGLE/renderer/vulkan/clspv_utils.h
        src/libANGLE/renderer/vulkan/vk_cl_utils.cpp
        src/libANGLE/renderer/vulkan/vk_cl_utils.h
    )
endif()

if(angle_use_vulkan_null_display)
    list(APPEND angle_vulkan_backend_sources
        src/libANGLE/renderer/vulkan/null/DisplayVkNull.cpp
        src/libANGLE/renderer/vulkan/null/DisplayVkNull.h
    )
endif()

set(angle_vulkan_backend_dma_buf_sources
    src/libANGLE/renderer/vulkan/linux/DeviceVkLinux.cpp
    src/libANGLE/renderer/vulkan/linux/DeviceVkLinux.h
    src/libANGLE/renderer/vulkan/linux/DisplayVkLinux.cpp
    src/libANGLE/renderer/vulkan/linux/DisplayVkLinux.h
    src/libANGLE/renderer/vulkan/linux/DisplayVkOffscreen.cpp
    src/libANGLE/renderer/vulkan/linux/DisplayVkOffscreen.h
    src/libANGLE/renderer/vulkan/linux/DmaBufImageSiblingVkLinux.cpp
    src/libANGLE/renderer/vulkan/linux/DmaBufImageSiblingVkLinux.h
    src/libANGLE/renderer/vulkan/linux/display/DisplayVkSimple.cpp
    src/libANGLE/renderer/vulkan/linux/display/DisplayVkSimple.h
    src/libANGLE/renderer/vulkan/linux/display/WindowSurfaceVkSimple.cpp
    src/libANGLE/renderer/vulkan/linux/display/WindowSurfaceVkSimple.h
    src/libANGLE/renderer/vulkan/linux/headless/DisplayVkHeadless.cpp
    src/libANGLE/renderer/vulkan/linux/headless/DisplayVkHeadless.h
    src/libANGLE/renderer/vulkan/linux/headless/WindowSurfaceVkHeadless.cpp
    src/libANGLE/renderer/vulkan/linux/headless/WindowSurfaceVkHeadless.h
)

set(angle_vulkan_backend_sources_win32
    # is_win
    src/libANGLE/renderer/vulkan/win32/DisplayVkWin32.cpp
    src/libANGLE/renderer/vulkan/win32/DisplayVkWin32.h
    src/libANGLE/renderer/vulkan/win32/WindowSurfaceVkWin32.cpp
    src/libANGLE/renderer/vulkan/win32/WindowSurfaceVkWin32.h
)

set(angle_vulkan_backend_sources_x11
    # angle_use_x11
    src/libANGLE/renderer/vulkan/linux/xcb/DisplayVkXcb.cpp
    src/libANGLE/renderer/vulkan/linux/xcb/DisplayVkXcb.h
    src/libANGLE/renderer/vulkan/linux/xcb/WindowSurfaceVkXcb.cpp
    src/libANGLE/renderer/vulkan/linux/xcb/WindowSurfaceVkXcb.h
)

set(angle_vulkan_backend_sources_wayland
    # angle_use_wayland
    src/libANGLE/renderer/vulkan/linux/wayland/DisplayVkWayland.cpp
    src/libANGLE/renderer/vulkan/linux/wayland/DisplayVkWayland.h
    src/libANGLE/renderer/vulkan/linux/wayland/WindowSurfaceVkWayland.cpp
    src/libANGLE/renderer/vulkan/linux/wayland/WindowSurfaceVkWayland.h
)

set(angle_vulkan_backend_sources_gbm
    # angle_use_gbm
    src/libANGLE/renderer/vulkan/linux/gbm/DisplayVkGbm.cpp
    src/libANGLE/renderer/vulkan/linux/gbm/DisplayVkGbm.h
)

set(angle_vulkan_backend_sources_mac
    # is_mac
    src/libANGLE/renderer/vulkan/mac/DisplayVkMac.h
    src/libANGLE/renderer/vulkan/mac/DisplayVkMac.mm
    src/libANGLE/renderer/vulkan/mac/IOSurfaceSurfaceVkMac.h
    src/libANGLE/renderer/vulkan/mac/IOSurfaceSurfaceVkMac.mm
    src/libANGLE/renderer/vulkan/mac/WindowSurfaceVkMac.h
    src/libANGLE/renderer/vulkan/mac/WindowSurfaceVkMac.mm
)

set(angle_vulkan_backend_sources_android
    src/libANGLE/renderer/vulkan/android/AHBFunctions.cpp
    src/libANGLE/renderer/vulkan/android/AHBFunctions.h
    src/libANGLE/renderer/vulkan/android/DisplayVkAndroid.cpp
    src/libANGLE/renderer/vulkan/android/DisplayVkAndroid.h
    src/libANGLE/renderer/vulkan/android/HardwareBufferImageSiblingVkAndroid.cpp
    src/libANGLE/renderer/vulkan/android/HardwareBufferImageSiblingVkAndroid.h
    src/libANGLE/renderer/vulkan/android/WindowSurfaceVkAndroid.cpp
    src/libANGLE/renderer/vulkan/android/WindowSurfaceVkAndroid.h
)

# src/libANGLE/renderer/vulkan/BUILD.gn
set(angle_vk_mem_alloc_wrapper_srcs
    src/libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.cpp
    src/libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.h
)

set(angle_d3d_shared_sources
    src/libANGLE/renderer/d3d/BufferD3D.cpp
    src/libANGLE/renderer/d3d/BufferD3D.h
    src/libANGLE/renderer/d3d/CompilerD3D.cpp
    src/libANGLE/renderer/d3d/CompilerD3D.h
    src/libANGLE/renderer/d3d/ContextD3D.h
    src/libANGLE/renderer/d3d/DisplayD3D.cpp
    src/libANGLE/renderer/d3d/DisplayD3D.h
    src/libANGLE/renderer/d3d/DynamicHLSL.cpp
    src/libANGLE/renderer/d3d/DynamicHLSL.h
    src/libANGLE/renderer/d3d/DynamicImage2DHLSL.cpp
    src/libANGLE/renderer/d3d/DynamicImage2DHLSL.h
    src/libANGLE/renderer/d3d/EGLImageD3D.cpp
    src/libANGLE/renderer/d3d/EGLImageD3D.h
    src/libANGLE/renderer/d3d/FramebufferD3D.cpp
    src/libANGLE/renderer/d3d/FramebufferD3D.h
    src/libANGLE/renderer/d3d/HLSLCompiler.cpp
    src/libANGLE/renderer/d3d/HLSLCompiler.h
    src/libANGLE/renderer/d3d/ImageD3D.cpp
    src/libANGLE/renderer/d3d/ImageD3D.h
    src/libANGLE/renderer/d3d/IndexBuffer.cpp
    src/libANGLE/renderer/d3d/IndexBuffer.h
    src/libANGLE/renderer/d3d/IndexDataManager.cpp
    src/libANGLE/renderer/d3d/IndexDataManager.h
    src/libANGLE/renderer/d3d/NativeWindowD3D.cpp
    src/libANGLE/renderer/d3d/NativeWindowD3D.h
    src/libANGLE/renderer/d3d/ProgramD3D.cpp
    src/libANGLE/renderer/d3d/ProgramD3D.h
    src/libANGLE/renderer/d3d/ProgramExecutableD3D.cpp
    src/libANGLE/renderer/d3d/ProgramExecutableD3D.h
    src/libANGLE/renderer/d3d/RenderTargetD3D.cpp
    src/libANGLE/renderer/d3d/RenderTargetD3D.h
    src/libANGLE/renderer/d3d/RenderbufferD3D.cpp
    src/libANGLE/renderer/d3d/RenderbufferD3D.h
    src/libANGLE/renderer/d3d/RendererD3D.cpp
    src/libANGLE/renderer/d3d/RendererD3D.h
    src/libANGLE/renderer/d3d/SamplerD3D.h
    src/libANGLE/renderer/d3d/ShaderD3D.cpp
    src/libANGLE/renderer/d3d/ShaderD3D.h
    src/libANGLE/renderer/d3d/ShaderExecutableD3D.cpp
    src/libANGLE/renderer/d3d/ShaderExecutableD3D.h
    src/libANGLE/renderer/d3d/SurfaceD3D.cpp
    src/libANGLE/renderer/d3d/SurfaceD3D.h
    src/libANGLE/renderer/d3d/SwapChainD3D.cpp
    src/libANGLE/renderer/d3d/SwapChainD3D.h
    src/libANGLE/renderer/d3d/TextureD3D.cpp
    src/libANGLE/renderer/d3d/TextureD3D.h
    src/libANGLE/renderer/d3d/TextureStorage.h
    src/libANGLE/renderer/d3d/VertexBuffer.cpp
    src/libANGLE/renderer/d3d/VertexBuffer.h
    src/libANGLE/renderer/d3d/VertexDataManager.cpp
    src/libANGLE/renderer/d3d/VertexDataManager.h
    src/libANGLE/renderer/d3d/driver_utils_d3d.cpp
    src/libANGLE/renderer/d3d/driver_utils_d3d.h
    src/libANGLE/renderer/d3d/formatutilsD3D.h
)

set(angle_d3d9_backend_sources
    # angle_enable_d3d9
    src/libANGLE/renderer/d3d/d3d9/Blit9.cpp
    src/libANGLE/renderer/d3d/d3d9/Blit9.h
    src/libANGLE/renderer/d3d/d3d9/Buffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/Buffer9.h
    src/libANGLE/renderer/d3d/d3d9/Context9.cpp
    src/libANGLE/renderer/d3d/d3d9/Context9.h
    src/libANGLE/renderer/d3d/d3d9/DebugAnnotator9.cpp
    src/libANGLE/renderer/d3d/d3d9/DebugAnnotator9.h
    src/libANGLE/renderer/d3d/d3d9/Device9.cpp
    src/libANGLE/renderer/d3d/d3d9/Device9.h
    src/libANGLE/renderer/d3d/d3d9/Fence9.cpp
    src/libANGLE/renderer/d3d/d3d9/Fence9.h
    src/libANGLE/renderer/d3d/d3d9/Framebuffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/Framebuffer9.h
    src/libANGLE/renderer/d3d/d3d9/Image9.cpp
    src/libANGLE/renderer/d3d/d3d9/Image9.h
    src/libANGLE/renderer/d3d/d3d9/IndexBuffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/IndexBuffer9.h
    src/libANGLE/renderer/d3d/d3d9/NativeWindow9.cpp
    src/libANGLE/renderer/d3d/d3d9/NativeWindow9.h
    src/libANGLE/renderer/d3d/d3d9/Query9.cpp
    src/libANGLE/renderer/d3d/d3d9/Query9.h
    src/libANGLE/renderer/d3d/d3d9/RenderTarget9.cpp
    src/libANGLE/renderer/d3d/d3d9/RenderTarget9.h
    src/libANGLE/renderer/d3d/d3d9/Renderer9.cpp
    src/libANGLE/renderer/d3d/d3d9/Renderer9.h
    src/libANGLE/renderer/d3d/d3d9/ShaderCache.h
    src/libANGLE/renderer/d3d/d3d9/ShaderExecutable9.cpp
    src/libANGLE/renderer/d3d/d3d9/ShaderExecutable9.h
    src/libANGLE/renderer/d3d/d3d9/StateManager9.cpp
    src/libANGLE/renderer/d3d/d3d9/StateManager9.h
    src/libANGLE/renderer/d3d/d3d9/SwapChain9.cpp
    src/libANGLE/renderer/d3d/d3d9/SwapChain9.h
    src/libANGLE/renderer/d3d/d3d9/TextureStorage9.cpp
    src/libANGLE/renderer/d3d/d3d9/TextureStorage9.h
    src/libANGLE/renderer/d3d/d3d9/VertexArray9.h
    src/libANGLE/renderer/d3d/d3d9/VertexBuffer9.cpp
    src/libANGLE/renderer/d3d/d3d9/VertexBuffer9.h
    src/libANGLE/renderer/d3d/d3d9/VertexDeclarationCache.cpp
    src/libANGLE/renderer/d3d/d3d9/VertexDeclarationCache.h
    src/libANGLE/renderer/d3d/d3d9/formatutils9.cpp
    src/libANGLE/renderer/d3d/d3d9/formatutils9.h
    src/libANGLE/renderer/d3d/d3d9/renderer9_utils.cpp
    src/libANGLE/renderer/d3d/d3d9/renderer9_utils.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/componentmaskpremultps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/componentmaskps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/componentmaskunmultps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/luminancepremultps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/luminanceps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/luminanceunmultps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/passthroughps.h
    src/libANGLE/renderer/d3d/d3d9/shaders/compiled/standardvs.h
    src/libANGLE/renderer/d3d/d3d9/vertexconversion.h
)

set(angle_d3d11_backend_sources
    src/libANGLE/renderer/d3d/d3d11/Blit11.cpp
    src/libANGLE/renderer/d3d/d3d11/Blit11.h
    src/libANGLE/renderer/d3d/d3d11/Blit11Helper_autogen.inc
    src/libANGLE/renderer/d3d/d3d11/Buffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/Buffer11.h
    src/libANGLE/renderer/d3d/d3d11/Clear11.cpp
    src/libANGLE/renderer/d3d/d3d11/Clear11.h
    src/libANGLE/renderer/d3d/d3d11/Context11.cpp
    src/libANGLE/renderer/d3d/d3d11/Context11.h
    src/libANGLE/renderer/d3d/d3d11/DebugAnnotator11.cpp
    src/libANGLE/renderer/d3d/d3d11/DebugAnnotator11.h
    src/libANGLE/renderer/d3d/d3d11/Device11.cpp
    src/libANGLE/renderer/d3d/d3d11/Device11.h
    src/libANGLE/renderer/d3d/d3d11/ExternalImageSiblingImpl11.cpp
    src/libANGLE/renderer/d3d/d3d11/ExternalImageSiblingImpl11.h
    src/libANGLE/renderer/d3d/d3d11/Fence11.cpp
    src/libANGLE/renderer/d3d/d3d11/Fence11.h
    src/libANGLE/renderer/d3d/d3d11/Framebuffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/Framebuffer11.h
    src/libANGLE/renderer/d3d/d3d11/Image11.cpp
    src/libANGLE/renderer/d3d/d3d11/Image11.h
    src/libANGLE/renderer/d3d/d3d11/IndexBuffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/IndexBuffer11.h
    src/libANGLE/renderer/d3d/d3d11/InputLayoutCache.cpp
    src/libANGLE/renderer/d3d/d3d11/InputLayoutCache.h
    src/libANGLE/renderer/d3d/d3d11/MappedSubresourceVerifier11.cpp
    src/libANGLE/renderer/d3d/d3d11/MappedSubresourceVerifier11.h
    src/libANGLE/renderer/d3d/d3d11/NativeWindow11.h
    src/libANGLE/renderer/d3d/d3d11/PixelTransfer11.cpp
    src/libANGLE/renderer/d3d/d3d11/PixelTransfer11.h
    src/libANGLE/renderer/d3d/d3d11/Program11.cpp
    src/libANGLE/renderer/d3d/d3d11/Program11.h
    src/libANGLE/renderer/d3d/d3d11/ProgramPipeline11.cpp
    src/libANGLE/renderer/d3d/d3d11/ProgramPipeline11.h
    src/libANGLE/renderer/d3d/d3d11/Query11.cpp
    src/libANGLE/renderer/d3d/d3d11/Query11.h
    src/libANGLE/renderer/d3d/d3d11/RenderStateCache.cpp
    src/libANGLE/renderer/d3d/d3d11/RenderStateCache.h
    src/libANGLE/renderer/d3d/d3d11/RenderTarget11.cpp
    src/libANGLE/renderer/d3d/d3d11/RenderTarget11.h
    src/libANGLE/renderer/d3d/d3d11/Renderer11.cpp
    src/libANGLE/renderer/d3d/d3d11/Renderer11.h
    src/libANGLE/renderer/d3d/d3d11/ResourceManager11.cpp
    src/libANGLE/renderer/d3d/d3d11/ResourceManager11.h
    src/libANGLE/renderer/d3d/d3d11/ShaderExecutable11.cpp
    src/libANGLE/renderer/d3d/d3d11/ShaderExecutable11.h
    src/libANGLE/renderer/d3d/d3d11/StateManager11.cpp
    src/libANGLE/renderer/d3d/d3d11/StateManager11.h
    src/libANGLE/renderer/d3d/d3d11/StreamProducerD3DTexture.cpp
    src/libANGLE/renderer/d3d/d3d11/StreamProducerD3DTexture.h
    src/libANGLE/renderer/d3d/d3d11/SwapChain11.cpp
    src/libANGLE/renderer/d3d/d3d11/SwapChain11.h
    src/libANGLE/renderer/d3d/d3d11/TextureStorage11.cpp
    src/libANGLE/renderer/d3d/d3d11/TextureStorage11.h
    src/libANGLE/renderer/d3d/d3d11/TransformFeedback11.cpp
    src/libANGLE/renderer/d3d/d3d11/TransformFeedback11.h
    src/libANGLE/renderer/d3d/d3d11/Trim11.cpp
    src/libANGLE/renderer/d3d/d3d11/Trim11.h
    src/libANGLE/renderer/d3d/d3d11/VertexArray11.cpp
    src/libANGLE/renderer/d3d/d3d11/VertexArray11.h
    src/libANGLE/renderer/d3d/d3d11/VertexBuffer11.cpp
    src/libANGLE/renderer/d3d/d3d11/VertexBuffer11.h
    src/libANGLE/renderer/d3d/d3d11/formatutils11.cpp
    src/libANGLE/renderer/d3d/d3d11/formatutils11.h
    src/libANGLE/renderer/d3d/d3d11/renderer11_utils.cpp
    src/libANGLE/renderer/d3d/d3d11/renderer11_utils.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/buffertotexture11_gs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/buffertotexture11_ps_4f.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/buffertotexture11_ps_4i.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/buffertotexture11_ps_4ui.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/buffertotexture11_vs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clear11multiviewgs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clear11multiviewvs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clear11vs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/cleardepth11ps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps1.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps2.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps3.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps4.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps5.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps6.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps7.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps8.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps1.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps2.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps3.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps4.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps5.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps6.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps7.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps8.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps1.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps2.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps3.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps4.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps5.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps6.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps7.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps8.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough2d11vs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough3d11gs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/passthrough3d11vs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughdepth2d11ps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/passthroughrgba2dms11ps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvecolor2dps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepth11_ps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepthstencil11_ps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvedepthstencil11_vs.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/resolvestencil11_ps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef2darrayps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef2dps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlef3dps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei2darrayps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei2dps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzlei3dps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui2darrayps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui2dps.h
    src/libANGLE/renderer/d3d/d3d11/shaders/compiled/swizzleui3dps.h
    src/libANGLE/renderer/d3d/d3d11/texture_format_table.cpp
    src/libANGLE/renderer/d3d/d3d11/texture_format_table.h
    src/libANGLE/renderer/d3d/d3d11/texture_format_table_autogen.cpp
    src/libANGLE/renderer/d3d/d3d11/texture_format_table_utils.h
)

set(angle_d3d11_backend_sources_winuwp
    # angle_is_winuwp
    src/libANGLE/renderer/d3d/d3d11/winrt/CoreWindowNativeWindow.cpp
    src/libANGLE/renderer/d3d/d3d11/winrt/CoreWindowNativeWindow.h
    src/libANGLE/renderer/d3d/d3d11/winrt/InspectableNativeWindow.cpp
    src/libANGLE/renderer/d3d/d3d11/winrt/InspectableNativeWindow.h
    src/libANGLE/renderer/d3d/d3d11/winrt/NativeWindow11WinRT.cpp
    src/libANGLE/renderer/d3d/d3d11/winrt/NativeWindow11WinRT.h
    src/libANGLE/renderer/d3d/d3d11/winrt/SwapChainPanelNativeWindow.cpp
    src/libANGLE/renderer/d3d/d3d11/winrt/SwapChainPanelNativeWindow.h
)

set(angle_d3d11_backend_sources_win32
    # !angle_is_winuwp
    src/libANGLE/renderer/d3d/d3d11/win32/NativeWindow11Win32.cpp
    src/libANGLE/renderer/d3d/d3d11/win32/NativeWindow11Win32.h
)

set(angle_d3d11_backend_sources_compositor_native_window
    # angle_enable_d3d11_compositor_native_window
    src/libANGLE/renderer/d3d/d3d11/converged/CompositorNativeWindow11.cpp
    src/libANGLE/renderer/d3d/d3d11/converged/CompositorNativeWindow11.h
)

set(angle_metal_backend_sources
    src/libANGLE/renderer/metal/BufferMtl.h
    src/libANGLE/renderer/metal/BufferMtl.mm
    src/libANGLE/renderer/metal/CompilerMtl.h
    src/libANGLE/renderer/metal/CompilerMtl.mm
    src/libANGLE/renderer/metal/ContextMtl.h
    src/libANGLE/renderer/metal/ContextMtl.mm
    src/libANGLE/renderer/metal/DeviceMtl.h
    src/libANGLE/renderer/metal/DeviceMtl.mm
    src/libANGLE/renderer/metal/DisplayMtl.h
    src/libANGLE/renderer/metal/DisplayMtl.mm
    src/libANGLE/renderer/metal/DisplayMtl_api.h
    src/libANGLE/renderer/metal/FrameBufferMtl.h
    src/libANGLE/renderer/metal/FrameBufferMtl.mm
    src/libANGLE/renderer/metal/IOSurfaceSurfaceMtl.h
    src/libANGLE/renderer/metal/IOSurfaceSurfaceMtl.mm
    src/libANGLE/renderer/metal/ImageMtl.h
    src/libANGLE/renderer/metal/ImageMtl.mm
    src/libANGLE/renderer/metal/ProgramExecutableMtl.h
    src/libANGLE/renderer/metal/ProgramExecutableMtl.mm
    src/libANGLE/renderer/metal/ProgramMtl.h
    src/libANGLE/renderer/metal/ProgramMtl.mm
    src/libANGLE/renderer/metal/ProvokingVertexHelper.h
    src/libANGLE/renderer/metal/ProvokingVertexHelper.mm
    src/libANGLE/renderer/metal/QueryMtl.h
    src/libANGLE/renderer/metal/QueryMtl.mm
    src/libANGLE/renderer/metal/RenderBufferMtl.h
    src/libANGLE/renderer/metal/RenderBufferMtl.mm
    src/libANGLE/renderer/metal/RenderTargetMtl.h
    src/libANGLE/renderer/metal/RenderTargetMtl.mm
    src/libANGLE/renderer/metal/SamplerMtl.h
    src/libANGLE/renderer/metal/SamplerMtl.mm
    src/libANGLE/renderer/metal/ShaderMtl.h
    src/libANGLE/renderer/metal/ShaderMtl.mm
    src/libANGLE/renderer/metal/SurfaceMtl.h
    src/libANGLE/renderer/metal/SurfaceMtl.mm
    src/libANGLE/renderer/metal/SyncMtl.h
    src/libANGLE/renderer/metal/SyncMtl.mm
    src/libANGLE/renderer/metal/TextureMtl.h
    src/libANGLE/renderer/metal/TextureMtl.mm
    src/libANGLE/renderer/metal/TransformFeedbackMtl.h
    src/libANGLE/renderer/metal/TransformFeedbackMtl.mm
    src/libANGLE/renderer/metal/VertexArrayMtl.h
    src/libANGLE/renderer/metal/VertexArrayMtl.mm
    src/libANGLE/renderer/metal/blocklayoutMetal.cpp
    src/libANGLE/renderer/metal/blocklayoutMetal.h
    src/libANGLE/renderer/metal/mtl_buffer_manager.h
    src/libANGLE/renderer/metal/mtl_buffer_manager.mm
    src/libANGLE/renderer/metal/mtl_buffer_pool.h
    src/libANGLE/renderer/metal/mtl_buffer_pool.mm
    src/libANGLE/renderer/metal/mtl_command_buffer.h
    src/libANGLE/renderer/metal/mtl_command_buffer.mm
    src/libANGLE/renderer/metal/mtl_common.h
    src/libANGLE/renderer/metal/mtl_common.mm
    src/libANGLE/renderer/metal/mtl_context_device.h
    src/libANGLE/renderer/metal/mtl_context_device.mm
    src/libANGLE/renderer/metal/mtl_format_table_autogen.mm
    src/libANGLE/renderer/metal/mtl_format_utils.h
    src/libANGLE/renderer/metal/mtl_format_utils.mm
    src/libANGLE/renderer/metal/mtl_library_cache.h
    src/libANGLE/renderer/metal/mtl_library_cache.mm
    src/libANGLE/renderer/metal/mtl_msl_utils.h
    src/libANGLE/renderer/metal/mtl_msl_utils.mm
    src/libANGLE/renderer/metal/mtl_occlusion_query_pool.h
    src/libANGLE/renderer/metal/mtl_occlusion_query_pool.mm
    src/libANGLE/renderer/metal/mtl_pipeline_cache.h
    src/libANGLE/renderer/metal/mtl_pipeline_cache.mm
    src/libANGLE/renderer/metal/mtl_render_utils.h
    src/libANGLE/renderer/metal/mtl_render_utils.mm
    src/libANGLE/renderer/metal/mtl_resource_spi.h
    src/libANGLE/renderer/metal/mtl_resources.h
    src/libANGLE/renderer/metal/mtl_resources.mm
    src/libANGLE/renderer/metal/mtl_state_cache.h
    src/libANGLE/renderer/metal/mtl_state_cache.mm
    src/libANGLE/renderer/metal/mtl_utils.h
    src/libANGLE/renderer/metal/mtl_utils.mm
    src/libANGLE/renderer/metal/renderermtl_utils.cpp
    src/libANGLE/renderer/metal/renderermtl_utils.h
    src/libANGLE/renderer/metal/shaders/constants.h
    src/libANGLE/renderer/metal/shaders/mtl_internal_shaders_src_autogen.h
    src/libANGLE/renderer/metal/shaders/rewrite_indices_shared.h
)

set(angle_wgpu_backend_sources
    src/libANGLE/renderer/wgpu/BufferWgpu.cpp
    src/libANGLE/renderer/wgpu/BufferWgpu.h
    src/libANGLE/renderer/wgpu/CompilerWgpu.cpp
    src/libANGLE/renderer/wgpu/CompilerWgpu.h
    src/libANGLE/renderer/wgpu/ContextWgpu.cpp
    src/libANGLE/renderer/wgpu/ContextWgpu.h
    src/libANGLE/renderer/wgpu/DeviceWgpu.cpp
    src/libANGLE/renderer/wgpu/DeviceWgpu.h
    src/libANGLE/renderer/wgpu/DisplayWgpu.cpp
    src/libANGLE/renderer/wgpu/DisplayWgpu.h
    src/libANGLE/renderer/wgpu/DisplayWgpu_api.h
    src/libANGLE/renderer/wgpu/FenceNVWgpu.cpp
    src/libANGLE/renderer/wgpu/FenceNVWgpu.h
    src/libANGLE/renderer/wgpu/FramebufferWgpu.cpp
    src/libANGLE/renderer/wgpu/FramebufferWgpu.h
    src/libANGLE/renderer/wgpu/ImageWgpu.cpp
    src/libANGLE/renderer/wgpu/ImageWgpu.h
    src/libANGLE/renderer/wgpu/ProgramExecutableWgpu.cpp
    src/libANGLE/renderer/wgpu/ProgramExecutableWgpu.h
    src/libANGLE/renderer/wgpu/ProgramPipelineWgpu.cpp
    src/libANGLE/renderer/wgpu/ProgramPipelineWgpu.h
    src/libANGLE/renderer/wgpu/ProgramWgpu.cpp
    src/libANGLE/renderer/wgpu/ProgramWgpu.h
    src/libANGLE/renderer/wgpu/QueryWgpu.cpp
    src/libANGLE/renderer/wgpu/QueryWgpu.h
    src/libANGLE/renderer/wgpu/RenderTargetWgpu.cpp
    src/libANGLE/renderer/wgpu/RenderTargetWgpu.h
    src/libANGLE/renderer/wgpu/RenderbufferWgpu.cpp
    src/libANGLE/renderer/wgpu/RenderbufferWgpu.h
    src/libANGLE/renderer/wgpu/SamplerWgpu.cpp
    src/libANGLE/renderer/wgpu/SamplerWgpu.h
    src/libANGLE/renderer/wgpu/ShaderWgpu.cpp
    src/libANGLE/renderer/wgpu/ShaderWgpu.h
    src/libANGLE/renderer/wgpu/SurfaceWgpu.cpp
    src/libANGLE/renderer/wgpu/SurfaceWgpu.h
    src/libANGLE/renderer/wgpu/SyncWgpu.cpp
    src/libANGLE/renderer/wgpu/SyncWgpu.h
    src/libANGLE/renderer/wgpu/TextureWgpu.cpp
    src/libANGLE/renderer/wgpu/TextureWgpu.h
    src/libANGLE/renderer/wgpu/TransformFeedbackWgpu.cpp
    src/libANGLE/renderer/wgpu/TransformFeedbackWgpu.h
    src/libANGLE/renderer/wgpu/VertexArrayWgpu.cpp
    src/libANGLE/renderer/wgpu/VertexArrayWgpu.h
    src/libANGLE/renderer/wgpu/wgpu_command_buffer.cpp
    src/libANGLE/renderer/wgpu/wgpu_command_buffer.h
    src/libANGLE/renderer/wgpu/wgpu_format_table_autogen.cpp
    src/libANGLE/renderer/wgpu/wgpu_format_utils.cpp
    src/libANGLE/renderer/wgpu/wgpu_format_utils.h
    src/libANGLE/renderer/wgpu/wgpu_helpers.cpp
    src/libANGLE/renderer/wgpu/wgpu_helpers.h
    src/libANGLE/renderer/wgpu/wgpu_pipeline_state.cpp
    src/libANGLE/renderer/wgpu/wgpu_pipeline_state.h
    src/libANGLE/renderer/wgpu/wgpu_utils.cpp
    src/libANGLE/renderer/wgpu/wgpu_utils.h
    src/libANGLE/renderer/wgpu/wgpu_wgsl_util.cpp
    src/libANGLE/renderer/wgpu/wgpu_wgsl_util.h
)

set(angle_wgpu_backend_sources_win32
    # is_win
    src/libANGLE/renderer/wgpu/win32/WindowSurfaceWgpuWin32.cpp
    src/libANGLE/renderer/wgpu/win32/WindowSurfaceWgpuWin32.h
)

set(angle_wgpu_backend_sources_mac
    # is_mac
    src/libANGLE/renderer/wgpu/mac/WindowSurfaceWgpuMetalLayer.h
    src/libANGLE/renderer/wgpu/mac/WindowSurfaceWgpuMetalLayer.mm
)

set(angle_wgpu_backend_sources_x11
    # angle_use_x11
    src/libANGLE/renderer/wgpu/linux/x11/WindowSurfaceWgpuX11.cpp
    src/libANGLE/renderer/wgpu/linux/x11/WindowSurfaceWgpuX11.h
)

# src/libGLESv2.gni

set(angle_libangle_common_headers
    src/common/BinaryStream.h
    src/common/CircularBuffer.h
    src/common/Color.h
    src/common/Color.inc
    src/common/CompiledShaderState.h
    src/common/FastVector.h
    src/common/FixedQueue.h
    src/common/FixedVector.h
    src/common/MemoryBuffer.h
    src/common/Optional.h
    src/common/PackedEGLEnums_autogen.h
    src/common/PackedEnums.h
    src/common/PackedGLEnums_autogen.h
    src/common/PackedCLEnums_autogen.h
    src/common/PoolAlloc.h
    src/common/RingBufferAllocator.h
    src/common/SimpleMutex.h
    src/common/SynchronizedValue.h
    src/common/WorkerThread.h
    src/common/aligned_memory.h
    src/common/android_util.h
    src/common/angleutils.h
    src/common/apple_platform_utils.h
    src/common/backtrace_utils.h
    src/common/base/anglebase/base_export.h
    src/common/base/anglebase/containers/mru_cache.h
    src/common/base/anglebase/logging.h
    src/common/base/anglebase/macros.h
    src/common/base/anglebase/no_destructor.h
    src/common/base/anglebase/numerics/checked_math.h
    src/common/base/anglebase/numerics/checked_math_impl.h
    src/common/base/anglebase/numerics/clamped_math.h
    src/common/base/anglebase/numerics/clamped_math_impl.h
    src/common/base/anglebase/numerics/math_constants.h
    src/common/base/anglebase/numerics/ranges.h
    src/common/base/anglebase/numerics/safe_conversions.h
    src/common/base/anglebase/numerics/safe_conversions_arm_impl.h
    src/common/base/anglebase/numerics/safe_conversions_impl.h
    src/common/base/anglebase/numerics/safe_math.h
    src/common/base/anglebase/numerics/safe_math_arm_impl.h
    src/common/base/anglebase/numerics/safe_math_clang_gcc_impl.h
    src/common/base/anglebase/numerics/safe_math_shared_impl.h
    src/common/base/anglebase/sha1.h
    src/common/base/anglebase/sys_byteorder.h
    src/common/bitset_utils.h
    src/common/debug.h
    src/common/entry_points_enum_autogen.h
    src/common/event_tracer.h
    src/common/hash_containers.h
    src/common/hash_utils.h
    src/common/log_utils.h
    src/common/mathutil.h
    src/common/matrix_utils.h
    src/common/platform.h
    src/common/platform_helpers.h
    src/common/span.h
    src/common/string_utils.h
    src/common/system_utils.h
    src/common/tls.h
    src/common/uniform_type_info_autogen.h
    src/common/utilities.h
    src/common/vector_utils.h
    src/libANGLE/CLBitField.h
)

set(angle_libangle_common_sources
    ${angle_libangle_common_headers}
    src/common/Float16ToFloat32.cpp
    src/common/MemoryBuffer.cpp
    src/common/PackedEGLEnums_autogen.cpp
    src/common/PackedEnums.cpp
    src/common/PackedGLEnums_autogen.cpp
    src/common/PoolAlloc.cpp
    src/common/RingBufferAllocator.cpp
    src/common/SimpleMutex.cpp
    src/common/WorkerThread.cpp
    src/common/aligned_memory.cpp
    src/common/android_util.cpp
    src/common/angleutils.cpp
    src/common/base/anglebase/sha1.cc
    src/common/debug.cpp
    src/common/entry_points_enum_autogen.cpp
    src/common/event_tracer.cpp
    src/common/mathutil.cpp
    src/common/matrix_utils.cpp
    src/common/platform_helpers.cpp
    src/common/string_utils.cpp
    src/common/system_utils.cpp
    src/common/tls.cpp
    src/common/uniform_type_info_autogen.cpp
    src/common/utilities.cpp
    # !angle_enable_unwind_backtrace_support
    src/common/backtrace_utils_noop.cpp
)

set(angle_libangle_common_shader_state_sources
    ${angle_libangle_common_headers}
    src/common/CompiledShaderState.cpp
)

set(angle_xxhash_sources
    src/common/third_party/xxhash/xxhash.c
    src/common/third_party/xxhash/xxhash.h
)

set(angle_libangle_common_sources_linux
    # is_linux || is_chromeos || is_android || is_fuchsia
    src/common/system_utils_linux.cpp
    src/common/system_utils_posix.cpp
)

set(angle_libangle_common_sources_apple
    # is_apple
    src/common/apple/ObjCPtr.h
    src/common/apple/SoftLinking.h
    src/common/apple/apple_platform.h
    src/common/apple_platform_utils.mm
    src/common/system_utils_apple.cpp
    src/common/system_utils_posix.cpp
)

set(angle_libangle_common_sources_mac
    # is_mac
    src/common/gl/cgl/FunctionsCGL.cpp
    src/common/gl/cgl/FunctionsCGL.h
    src/common/system_utils_mac.cpp
)

set(angle_libangle_common_sources_ios
    # is_ios
    src/common/system_utils_ios.cpp
)

set(angle_libangle_common_sources_winuwp
    src/common/system_utils_win.cpp
    src/common/system_utils_winuwp.cpp
)

set(angle_libangle_common_sources_win32
    src/common/system_utils_win.cpp
    src/common/system_utils_win32.cpp
)

set(angle_libangle_mac_sources
    src/libANGLE/renderer/driver_utils_mac.mm
)

set(angle_libangle_image_util_headers
    src/image_util/AstcDecompressor.h
    src/image_util/copyimage.h
    src/image_util/copyimage.inc
    src/image_util/generatemip.h
    src/image_util/generatemip.inc
    src/image_util/imageformats.h
    src/image_util/loadimage.h
    src/image_util/loadimage.inc
    src/image_util/storeimage.h
)

set(angle_libangle_image_util_sources
    src/image_util/copyimage.cpp
    src/image_util/imageformats.cpp
    src/image_util/loadimage.cpp
    src/image_util/loadimage_astc.cpp
    src/image_util/loadimage_etc.cpp
    src/image_util/loadimage_paletted.cpp
    src/image_util/storeimage_paletted.cpp
    # !angle_has_astc_encoder
    src/image_util/AstcDecompressorNoOp.cpp
)

set(angle_libangle_libangle_gpu_info_util_sources
    src/gpu_info_util/SystemInfo.cpp
    src/gpu_info_util/SystemInfo.h
    src/gpu_info_util/SystemInfo_internal.h
)

set(angle_libangle_gpu_info_util_win_sources
    src/gpu_info_util/SystemInfo_win.cpp
)

set(angle_libangle_gpu_info_util_android_sources
    src/gpu_info_util/SystemInfo_android.cpp
)

set(angle_libangle_gpu_info_util_linux_sources
    src/gpu_info_util/SystemInfo_linux.cpp
)

set(angle_libangle_gpu_info_util_vulkan_sources
    src/gpu_info_util/SystemInfo_vulkan.cpp
    src/gpu_info_util/SystemInfo_vulkan.h
)
set(angle_libangle_gpu_info_util_libpci_sources
    src/gpu_info_util/SystemInfo_libpci.cpp
)

set(angle_libangle_gpu_info_util_x11_sources
    src/gpu_info_util/SystemInfo_x11.cpp
)

set(angle_libangle_gpu_info_util_mac_sources
    src/gpu_info_util/SystemInfo_apple.mm
    src/gpu_info_util/SystemInfo_macos.mm
)

set(angle_libangle_gpu_info_util_ios_sources
    src/gpu_info_util/SystemInfo_apple.mm
    src/gpu_info_util/SystemInfo_ios.cpp
)

set(angle_libangle_includes
    include/angle_gl.h
    include/export.h
    include/EGL/egl.h
    include/EGL/eglext.h
    include/EGL/eglext_angle.h
    include/EGL/eglplatform.h
    include/GLES/gl.h
    include/GLES/glplatform.h
    include/GLES/glext.h
    include/GLES2/gl2.h
    include/GLES2/gl2ext.h
    include/GLES2/gl2ext_angle.h
    include/GLES2/gl2platform.h
    include/GLES3/gl3.h
    include/GLES3/gl3platform.h
    include/GLES3/gl31.h
    include/GLES3/gl32.h
    include/GLX/glxext.h
    include/KHR/khrplatform.h
    include/WGL/wgl.h
    include/platform/Feature.h
    include/platform/autogen/FeaturesD3D_autogen.h
    include/platform/autogen/FeaturesGL_autogen.h
    include/platform/autogen/FeaturesMtl_autogen.h
    include/platform/autogen/FeaturesVk_autogen.h
    include/platform/autogen/FrontendFeatures_autogen.h
    include/platform/PlatformMethods.h
    include/vulkan/vulkan_fuchsia_ext.h
)

set(angle_libangle_headers
    src/libANGLE/AttributeMap.h
    src/libANGLE/BlobCache.h
    src/libANGLE/Buffer.h
    src/libANGLE/Caps.h
    src/libANGLE/CLBitField.h
    src/libANGLE/CLRefPointer.h
    src/libANGLE/Compiler.h
    src/libANGLE/Config.h
    src/libANGLE/Constants.h
    src/libANGLE/Context.h
    src/libANGLE/Context.inl.h
    src/libANGLE/Context_gles_1_0_autogen.h
    src/libANGLE/Context_gles_2_0_autogen.h
    src/libANGLE/Context_gles_3_0_autogen.h
    src/libANGLE/Context_gles_3_1_autogen.h
    src/libANGLE/Context_gles_3_2_autogen.h
    src/libANGLE/Context_gles_ext_autogen.h
    src/libANGLE/Debug.h
    src/libANGLE/Device.h
    src/libANGLE/Display.h
    src/libANGLE/EGLSync.h
    src/libANGLE/Error.h
    src/libANGLE/Error.inc
    src/libANGLE/ErrorStrings.h
    src/libANGLE/Fence.h
    src/libANGLE/Framebuffer.h
    src/libANGLE/FramebufferAttachment.h
    src/libANGLE/GLES1Renderer.h
    src/libANGLE/GLES1Shaders.inc
    src/libANGLE/GLES1State.h
    src/libANGLE/GlobalMutex.h
    src/libANGLE/HandleAllocator.h
    src/libANGLE/Image.h
    src/libANGLE/ImageIndex.h
    src/libANGLE/IndexRangeCache.h
    src/libANGLE/InfoLog.h
    src/libANGLE/LoggingAnnotator.h
    src/libANGLE/MemoryObject.h
    src/libANGLE/MemoryProgramCache.h
    src/libANGLE/MemoryShaderCache.h
    src/libANGLE/Observer.h
    src/libANGLE/Overlay.h
    src/libANGLE/OverlayWidgets.h
    src/libANGLE/Overlay_autogen.h
    src/libANGLE/Overlay_font_autogen.h
    src/libANGLE/PixelLocalStorage.h
    src/libANGLE/Program.h
    src/libANGLE/ProgramExecutable.h
    src/libANGLE/ProgramLinkedResources.h
    src/libANGLE/ProgramPipeline.h
    src/libANGLE/Query.h
    src/libANGLE/RefCountObject.h
    src/libANGLE/Renderbuffer.h
    src/libANGLE/ResourceManager.h
    src/libANGLE/ResourceMap.h
    src/libANGLE/Sampler.h
    src/libANGLE/Semaphore.h
    src/libANGLE/Shader.h
    src/libANGLE/ShareGroup.h
    src/libANGLE/ContextMutex.h
    src/libANGLE/SizedMRUCache.h
    src/libANGLE/State.h
    src/libANGLE/Stream.h
    src/libANGLE/Surface.h
    src/libANGLE/Texture.h
    src/libANGLE/Thread.h
    src/libANGLE/TransformFeedback.h
    src/libANGLE/Uniform.h
    src/libANGLE/VaryingPacking.h
    src/libANGLE/Version.h
    src/libANGLE/Version.inc
    src/libANGLE/VertexArray.h
    src/libANGLE/VertexAttribute.h
    src/libANGLE/VertexAttribute.inc
    src/libANGLE/angletypes.h
    src/libANGLE/angletypes.inc
    src/libANGLE/cl_types.h
    src/libANGLE/context_private_call.inl.h
    src/libANGLE/context_private_call_autogen.h
    src/libANGLE/entry_points_utils.cpp
    src/libANGLE/entry_points_utils.h
    src/libANGLE/features.h
    src/libANGLE/formatutils.h
    src/libANGLE/gles_extensions_autogen.h
    src/libANGLE/histogram_macros.h
    src/libANGLE/queryconversions.h
    src/libANGLE/queryutils.h
    src/libANGLE/trace.h
    src/libANGLE/renderer/BufferImpl.h
    src/libANGLE/renderer/CompilerImpl.h
    src/libANGLE/renderer/ContextImpl.h
    src/libANGLE/renderer/driver_utils.h
    src/libANGLE/renderer/DeviceImpl.h
    src/libANGLE/renderer/DisplayImpl.h
    src/libANGLE/renderer/EGLImplFactory.h
    src/libANGLE/renderer/EGLReusableSync.h
    src/libANGLE/renderer/EGLSyncImpl.h
    src/libANGLE/renderer/FenceNVImpl.h
    src/libANGLE/renderer/FormatID_autogen.h
    src/libANGLE/renderer/Format.h
    src/libANGLE/renderer/FramebufferAttachmentObjectImpl.h
    src/libANGLE/renderer/FramebufferImpl.h
    src/libANGLE/renderer/GLImplFactory.h
    src/libANGLE/renderer/gl/functionsgl_enums.h
    src/libANGLE/renderer/ImageImpl.h
    src/libANGLE/renderer/MemoryObjectImpl.h
    src/libANGLE/renderer/OverlayImpl.h
    src/libANGLE/renderer/ProgramImpl.h
    src/libANGLE/renderer/ProgramExecutableImpl.h
    src/libANGLE/renderer/ProgramPipelineImpl.h
    src/libANGLE/renderer/QueryImpl.h
    src/libANGLE/renderer/RenderbufferImpl.h
    src/libANGLE/renderer/RenderTargetCache.h
    src/libANGLE/renderer/SamplerImpl.h
    src/libANGLE/renderer/SemaphoreImpl.h
    src/libANGLE/renderer/ShaderImpl.h
    src/libANGLE/renderer/ShareGroupImpl.h
    src/libANGLE/renderer/StreamProducerImpl.h
    src/libANGLE/renderer/SurfaceImpl.h
    src/libANGLE/renderer/SyncImpl.h
    src/libANGLE/renderer/TransformFeedbackImpl.h
    src/libANGLE/renderer/VertexArrayImpl.h
    src/libANGLE/renderer/vulkan/DisplayVk_api.h
    src/libANGLE/renderer/copyvertex.h
    src/libANGLE/renderer/copyvertex.inc.h
    src/libANGLE/renderer/load_functions_table.h
    src/libANGLE/renderer/renderer_utils.h
    src/libANGLE/renderer/serial_utils.h
    src/libANGLE/validationEGL.h
    src/libANGLE/validationEGL_autogen.h
    src/libANGLE/validationES.h
    src/libANGLE/validationES1.h
    src/libANGLE/validationES1_autogen.h
    src/libANGLE/validationES2.h
    src/libANGLE/validationES2_autogen.h
    src/libANGLE/validationES3.h
    src/libANGLE/validationES31.h
    src/libANGLE/validationES31_autogen.h
    src/libANGLE/validationES32.h
    src/libANGLE/validationES32_autogen.h
    src/libANGLE/validationES3_autogen.h
    src/libANGLE/validationESEXT.h
    src/libANGLE/validationESEXT_autogen.h
    src/common/base/anglebase/trace_event/trace_event.h
    src/libANGLE/CLPlatform.h
    src/libANGLE/renderer/CLPlatformImpl.h
    src/libANGLE/CLObject.h
    src/libANGLE/renderer/CLContextImpl.h
    src/libANGLE/renderer/CLDeviceImpl.h
    src/libANGLE/renderer/CLExtensions.h
    src/libANGLE/renderer/CLCommandQueueImpl.h
    src/libANGLE/renderer/CLEventImpl.h
    src/libANGLE/renderer/CLMemoryImpl.h
    src/libANGLE/renderer/CLProgramImpl.h
    src/libANGLE/renderer/CLSamplerImpl.h
    src/libANGLE/renderer/CLKernelImpl.h
)

set(angle_libangle_sources
    src/libANGLE/AttributeMap.cpp
    src/libANGLE/BlobCache.cpp
    src/libANGLE/Buffer.cpp
    src/libANGLE/Caps.cpp
    src/libANGLE/Compiler.cpp
    src/libANGLE/Config.cpp
    src/libANGLE/Context.cpp
    src/libANGLE/ContextMutex.cpp
    src/libANGLE/Context_gles_1_0.cpp
    src/libANGLE/Debug.cpp
    src/libANGLE/Device.cpp
    src/libANGLE/Display.cpp
    src/libANGLE/EGLSync.cpp
    src/libANGLE/Error.cpp
    src/libANGLE/Fence.cpp
    src/libANGLE/Framebuffer.cpp
    src/libANGLE/FramebufferAttachment.cpp
    src/libANGLE/GLES1Renderer.cpp
    src/libANGLE/GLES1State.cpp
    src/libANGLE/GlobalMutex.cpp
    src/libANGLE/HandleAllocator.cpp
    src/libANGLE/Image.cpp
    src/libANGLE/ImageIndex.cpp
    src/libANGLE/IndexRangeCache.cpp
    src/libANGLE/LoggingAnnotator.cpp
    src/libANGLE/MemoryObject.cpp
    src/libANGLE/MemoryProgramCache.cpp
    src/libANGLE/MemoryShaderCache.cpp
    src/libANGLE/Observer.cpp
    src/libANGLE/Overlay.cpp
    src/libANGLE/OverlayWidgets.cpp
    src/libANGLE/Overlay_autogen.cpp
    src/libANGLE/Overlay_font_autogen.cpp
    src/libANGLE/PixelLocalStorage.cpp
    src/libANGLE/Platform.cpp
    src/libANGLE/Program.cpp
    src/libANGLE/ProgramExecutable.cpp
    src/libANGLE/ProgramLinkedResources.cpp
    src/libANGLE/ProgramPipeline.cpp
    src/libANGLE/Query.cpp
    src/libANGLE/Renderbuffer.cpp
    src/libANGLE/ResourceManager.cpp
    src/libANGLE/Sampler.cpp
    src/libANGLE/Semaphore.cpp
    src/libANGLE/Shader.cpp
    src/libANGLE/ShareGroup.cpp
    src/libANGLE/State.cpp
    src/libANGLE/Stream.cpp
    src/libANGLE/Surface.cpp
    src/libANGLE/Texture.cpp
    src/libANGLE/Thread.cpp
    src/libANGLE/TransformFeedback.cpp
    src/libANGLE/Uniform.cpp
    src/libANGLE/VaryingPacking.cpp
    src/libANGLE/VertexArray.cpp
    src/libANGLE/VertexAttribute.cpp
    src/libANGLE/angletypes.cpp
    src/libANGLE/es3_copy_conversion_table_autogen.cpp
    src/libANGLE/format_map_autogen.cpp
    src/libANGLE/formatutils.cpp
    src/libANGLE/gles_extensions_autogen.cpp
    src/libANGLE/queryconversions.cpp
    src/libANGLE/queryutils.cpp
    src/libANGLE/renderer/BufferImpl.cpp
    src/libANGLE/renderer/ContextImpl.cpp
    src/libANGLE/renderer/DeviceImpl.cpp
    src/libANGLE/renderer/DisplayImpl.cpp
    src/libANGLE/renderer/EGLReusableSync.cpp
    src/libANGLE/renderer/EGLSyncImpl.cpp
    src/libANGLE/renderer/Format_table_autogen.cpp
    src/libANGLE/renderer/FramebufferImpl.cpp
    src/libANGLE/renderer/ImageImpl.cpp
    src/libANGLE/renderer/ProgramImpl.cpp
    src/libANGLE/renderer/ProgramPipelineImpl.cpp
    src/libANGLE/renderer/QueryImpl.cpp
    src/libANGLE/renderer/RenderbufferImpl.cpp
    src/libANGLE/renderer/ShaderImpl.cpp
    src/libANGLE/renderer/SurfaceImpl.cpp
    src/libANGLE/renderer/TextureImpl.cpp
    src/libANGLE/renderer/TransformFeedbackImpl.cpp
    src/libANGLE/renderer/VertexArrayImpl.cpp
    src/libANGLE/renderer/driver_utils.cpp
    src/libANGLE/renderer/load_functions_table_autogen.cpp
    src/libANGLE/renderer/renderer_utils.cpp
    src/libANGLE/validationEGL.cpp
    src/libANGLE/validationES.cpp
    src/libANGLE/validationES1.cpp
    src/libANGLE/validationES2.cpp
    src/libANGLE/validationES3.cpp
    src/libANGLE/validationES31.cpp
    src/libANGLE/validationES32.cpp
    src/libANGLE/validationESEXT.cpp
)

set(angle_libglesv2_sources
    src/libGLESv2/egl_context_lock_autogen.h
    src/libGLESv2/egl_context_lock_impl.h
    src/libGLESv2/egl_ext_stubs.cpp
    src/libGLESv2/egl_ext_stubs_autogen.h
    src/libGLESv2/egl_stubs.cpp
    src/libGLESv2/egl_stubs_autogen.h
    src/libGLESv2/egl_stubs_getprocaddress_autogen.cpp
    src/libGLESv2/entry_points_egl_autogen.cpp
    src/libGLESv2/entry_points_egl_autogen.h
    src/libGLESv2/entry_points_egl_ext_autogen.cpp
    src/libGLESv2/entry_points_egl_ext_autogen.h
    src/libGLESv2/entry_points_gles_1_0_autogen.cpp
    src/libGLESv2/entry_points_gles_1_0_autogen.h
    src/libGLESv2/entry_points_gles_2_0_autogen.cpp
    src/libGLESv2/entry_points_gles_2_0_autogen.h
    src/libGLESv2/entry_points_gles_3_0_autogen.cpp
    src/libGLESv2/entry_points_gles_3_0_autogen.h
    src/libGLESv2/entry_points_gles_3_1_autogen.cpp
    src/libGLESv2/entry_points_gles_3_1_autogen.h
    src/libGLESv2/entry_points_gles_3_2_autogen.cpp
    src/libGLESv2/entry_points_gles_3_2_autogen.h
    src/libGLESv2/entry_points_gles_ext_autogen.cpp
    src/libGLESv2/entry_points_gles_ext_autogen.h
    src/libGLESv2/global_state.cpp
    src/libGLESv2/global_state.h
    src/libGLESv2/libGLESv2_autogen.cpp
    src/libGLESv2/resource.h
)

set(angle_libglesv1_cm_sources
    src/libGLESv1_CM/libGLESv1_CM.cpp
    src/libGLESv1_CM/resource.h
)

set(angle_libegl_sources
    src/libEGL/egl_loader_autogen.h
    src/libEGL/libEGL_autogen.cpp
    src/libEGL/resource.h
    src/libGLESv2/entry_points_egl_autogen.h
    src/libGLESv2/entry_points_egl_ext_autogen.h
)

# src/common/spirv/BUILD.gn
set(angle_spirv_srcs
    # angle_spirv_base
    src/common/spirv/angle_spirv_utils.cpp
    # angle_spirv_builder
    src/common/spirv/spirv_instruction_builder_autogen.cpp
    src/common/spirv/spirv_instruction_builder_autogen.h
    # angle_spirv_parser
    src/common/spirv/spirv_instruction_parser_autogen.cpp
    src/common/spirv/spirv_instruction_parser_autogen.h
)

# src/common/linux/BUILD.gn
set(angle_dma_buf_srcs
    src/common/linux/dma_buf_utils.cpp
    src/common/linux/dma_buf_utils.h
)

# src/common/vulkan/BUILD.gn
set(angle_vulkan_icd_srcs
    src/common/vulkan/vulkan_icd.cpp
    src/common/vulkan/vulkan_icd.h
)
set(angle_libvulkan_loader_srcs
    src/common/vulkan/libvulkan_loader.cpp
    src/common/vulkan/libvulkan_loader.h
)


# src/BUILD.gn
set(angle_version_info_srcs
    src/common/angle_version_info.cpp
    src/common/angle_version_info.h
)

# BUILD.gn
set(angle_frame_capture_mock_srcs
    src/common/frame_capture_utils.h
    src/common/frame_capture_utils_autogen.h
    src/common/gl_enum_utils_autogen.h
    src/libANGLE/capture/FrameCapture.h
    src/libANGLE/capture/FrameCapture_mock.cpp
    src/libANGLE/capture/serialize.h
    src/libANGLE/capture/serialize_mock.cpp
)

set(angle_gl_enum_utils
    src/common/gl_enum_utils.cpp
    src/common/gl_enum_utils.h
    src/common/gl_enum_utils_autogen.cpp
    src/common/gl_enum_utils_autogen.h
)

set(angle_d3d_format_tables
    src/libANGLE/renderer/dxgi_format_map.h
    src/libANGLE/renderer/dxgi_format_map_autogen.cpp
    src/libANGLE/renderer/dxgi_support_table.h
    src/libANGLE/renderer/dxgi_support_table_autogen.cpp
)

set(angle_d3d_format_tables_nowinuwp
    src/libANGLE/renderer/d3d_format.cpp
    src/libANGLE/renderer/d3d_format.h
)

set(srcs)

include_directories(${ANGLE_ROOT}/src/common/base
    ${ANGLE_ROOT}/src/common/third_party/xxhash
    )
if(ANGLE_VULKAN_HEADERS_ROOT)
    include_directories(${ANGLE_VULKAN_HEADERS_ROOT}/include)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/include/vulkan/vulkan.h")
    set(ANGLE_VULKAN_HEADERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader")
    include_directories(${ANGLE_VULKAN_HEADERS_ROOT}/include)
endif()
if(ANGLE_SPIRV_HEADERS_ROOT)
    include_directories(${ANGLE_SPIRV_HEADERS_ROOT}/include)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/third_party/SPIRV-Headers/include")
    # Reuse the checked-out headers from the outer project for Vulkan builds.
    # The vendor trees remain read-only inputs.
    set(ANGLE_SPIRV_HEADERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/third_party/SPIRV-Headers")
    include_directories(${ANGLE_SPIRV_HEADERS_ROOT}/include)
endif()
if(ANGLE_SPIRV_TOOLS_ROOT)
    include_directories(${ANGLE_SPIRV_TOOLS_ROOT}/include)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/third_party/SPIRV-Tools/include")
    set(ANGLE_SPIRV_TOOLS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/third_party/SPIRV-Tools")
    include_directories(${ANGLE_SPIRV_TOOLS_ROOT}/include)
endif()
if(ANGLE_GLSLANG_ROOT)
    include_directories(${ANGLE_GLSLANG_ROOT})
endif()
if(ANGLE_VOLK_ROOT)
    include_directories(${ANGLE_VOLK_ROOT})
endif()
if(ANGLE_SPIRV_CROSS_ROOT)
    include_directories(${ANGLE_SPIRV_CROSS_ROOT})
endif()

list(APPEND srcs
    ${angle_version_info_srcs}
    ${angle_frame_capture_mock_srcs}
    ${angle_preprocessor_sources}
    ${angle_translator_sources}
    ${angle_translator_essl_symbol_table_sources}
    ${angle_libangle_common_sources}
    ${angle_libangle_common_shader_state_sources}
    ${angle_libangle_sources}
    ${angle_xxhash_sources}
    ${angle_libangle_image_util_sources}
    ${angle_libglesv2_sources}
    ${angle_libangle_libangle_gpu_info_util_sources}
    ${angle_gl_enum_utils}
)

if(NOT ANGLE_EXCLUDE_ENTRYPOINTS)
    message(STATUS "WARING: Implement split entrypoint mode!")
    #    list(APPEND srcs
    #    ${angle_libglesv2_entrypoints_srcs})
endif()

# debug
#list(APPEND srcs
#    ${angle_gl_enum_utils})

if(angle_enable_vk)
    list(APPEND srcs
        ${angle_translator_lib_spirv_sources}
        ${angle_vulkan_backend_sources}
        ${angle_spirv_srcs}
        ${angle_vulkan_icd_srcs}
        ${angle_libangle_gpu_info_util_vulkan_sources}
        ${angle_libvulkan_loader_srcs})
    if(ANGLE_VMA_ROOT)
        # Use the AnyGL-level VMA wrapper instead of the vendor one: it
        # supplies vkGetInstanceProcAddr/vkGetDeviceProcAddr (via volk),
        # which VMA 3.4+ requires in dynamic-function mode.
        list(APPEND srcs ${CMAKE_CURRENT_SOURCE_DIR}/vma_wrapper.cpp)
    else()
        message(STATUS "ANGLE_VMA_ROOT not set; Vulkan VMA wrapper will use ANGLE fallback")
    endif()
endif()
if(angle_enable_dx)
    list(APPEND srcs
        ${angle_translator_hlsl_sources}
        ${angle_d3d_shared_sources}
        ${angle_d3d11_backend_sources})
endif()
if(angle_enable_gl)
    # Native OpenGL backend: enable the backend and collect its sources.
    # Linux uses the EGL platform sources (DisplayEGL); GLX sources are
    # only needed for the desktop-GL path and are skipped here.
    add_definitions(-DANGLE_ENABLE_OPENGL -DANGLE_ENABLE_GL_DESKTOP_BACKEND
        -DANGLE_ENABLE_ESSL -DANGLE_ENABLE_GLSL)
    list(APPEND srcs
        ${angle_translator_glsl_base_sources}
        ${angle_translator_glsl_and_vulkan_base_sources}
        ${angle_translator_essl_sources}
        ${angle_translator_glsl_sources}
        ${ANGLE_ROOT}/src/compiler/translator/tree_ops/glsl/ExpandFragmentOutputsToVec4.cpp
        ${ANGLE_ROOT}/src/compiler/translator/tree_ops/glsl/WrapStructConstructors.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/BlitGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/BufferGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ClearMultiviewGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/CompilerGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ContextGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/DispatchTableGL_autogen.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/DisplayGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/FenceNVGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/FramebufferGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/FunctionsGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ImageGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/MemoryObjectGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ProgramExecutableGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ProgramGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ProgramPipelineGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/QueryGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/RenderbufferGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/RendererGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/SamplerGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/SemaphoreGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/ShaderGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/StateManagerGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/SurfaceGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/SyncGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/TextureGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/TransformFeedbackGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/VertexArrayGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/formatutilsgl.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/renderergl_utils.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/ContextEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/DeviceEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/DisplayEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/DmaBufImageSiblingEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/FunctionsEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/FunctionsEGLDL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/ImageEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/PbufferSurfaceEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/RendererEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/SurfaceEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/SyncEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/WindowSurfaceEGL.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/egl/egl_utils.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/ContextGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/DisplayGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/DisplayGLX_api.h
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/FunctionsGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/PbufferSurfaceGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/PixmapSurfaceGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/RendererGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/SurfaceGLX.h
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/WindowSurfaceGLX.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/functionsglx_typedefs.h
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/glx_utils.cpp
        ${ANGLE_ROOT}/src/libANGLE/renderer/gl/glx/platform_glx.h
    )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
    # Assume UWP
    list(APPEND srcs
        ${angle_d3d_format_tables}
        ${angle_libangle_common_sources_winuwp}
        ${angle_d3d11_backend_sources_winuwp}
        ${angle_d3d11_backend_sources_compositor_native_window})
    add_definitions(
        -DANGLE_ENABLE_WINDOWS_UWP
        -DANGLE_ENABLE_D3D11_COMPOSITOR_NATIVE_WINDOW
        -DANGLE_IS_WINUWP
        -D_SCL_SECURE_NO_WARNINGS
        -D_CRT_SECURE_NO_WARNINGS)
elseif(WIN32)
    list(APPEND srcs
        ${angle_libangle_common_sources_win32})
    if(angle_enable_vk)
        list(APPEND srcs
            ${angle_vulkan_backend_sources_win32})
    endif()
    if(angle_enable_dx)
        list(APPEND srcs
            ${angle_d3d_format_tables}
            ${angle_d3d_format_tables_nowinuwp}
            ${angle_d3d11_backend_sources_win32})
        add_definitions(-DANGLE_ENABLE_D3D11)
    endif()

    # MinGW header bug workaround
    # https://github.com/msys2/MINGW-packages/issues/22160
    add_definitions(-D____FIReference_1_boolean_INTERFACE_DEFINED__)
elseif(APPLE)
    list(APPEND srcs
        ${angle_libangle_common_sources_apple})
    if(angle_enable_vk)
        list(APPEND srcs ${angle_vulkan_backend_sources_mac})
    endif()
    if(DEFINED ANGLE_BACKEND AND ANGLE_BACKEND STREQUAL Metal)
        list(APPEND srcs ${angle_metal_backend_sources})
    endif()
    if("${CMAKE_SYSTEM_NAME}" STREQUAL iOS OR
            "${CMAKE_SYSTEM_NAME}" STREQUAL tvOS OR
            "${CMAKE_SYSTEM_NAME}" STREQUAL visionOS)
        list(APPEND srcs
            ${angle_libangle_common_sources_ios}
            ${angle_libangle_gpu_info_util_ios_sources})
    else()
        list(APPEND srcs
            ${angle_libangle_mac_sources}
            ${angle_libangle_common_sources_mac}
            ${angle_libangle_gpu_info_util_mac_sources})
    endif()
    add_definitions(
        -Wno-nullability-completeness
        -Wno-unguarded-availability
        -fno-objc-arc
        )
elseif(UNIX)
    # Linux
    list(APPEND srcs
        ${angle_libangle_common_sources_linux}
        ${angle_libangle_gpu_info_util_linux_sources}
        ${angle_dma_buf_srcs}
        )
    # Platform identity for both the GL and Vulkan backends.
    add_definitions(-DANGLE_PLATFORM_LINUX -DANGLE_USE_X11)
    if(angle_enable_vk)
        if(ANDROID)
            add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)
            message(STATUS "FIXME: DisplayVkAndroid")
            list(APPEND srcs
                ${angle_libangle_gpu_info_util_android_sources}
                ${angle_vulkan_backend_sources_android}
            )
        else()
            add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR
                -DANGLE_USE_GBM
                -DANGLE_USE_WAYLAND
                -DANGLE_USE_VULKAN_DISPLAY # Do we need this?
            )
            add_definitions(-DVK_USE_PLATFORM_XCB_KHR)

            list(APPEND srcs 
                ${angle_vulkan_backend_sources_wayland}
                ${angle_vulkan_backend_dma_buf_sources}
                ${angle_vulkan_backend_sources_gbm}
                ${angle_vulkan_backend_sources_x11}
            )
        endif()
    endif()
endif()

# Rebase sources
function(rebase_sources var)
    set(srcs0)
    foreach(s ${ARGN})
        if("${s}" MATCHES "^/" OR "${s}" MATCHES "^.:/")
            set(srcname ${s})
        else()
            set(srcname ${ANGLE_ROOT}/${s})
        endif()
        list(APPEND srcs0 ${srcname})
    endforeach()
    set(${var} ${srcs0} PARENT_SCOPE)
endfunction()

# Rebase the source groups imported from the ANGLE GN manifests.  The
# checkout may intentionally omit optional GN-only files, so ignore only
# manifest entries that are not present; no directory globbing is used.
function(rebase_existing_sources var)
    set(result)
    foreach(s ${ARGN})
        if("${s}" MATCHES "^/" OR "${s}" MATCHES "^.:/")
            set(candidate "${s}")
        else()
            set(candidate "${ANGLE_ROOT}/${s}")
        endif()
        if(EXISTS "${candidate}")
            list(APPEND result "${candidate}")
        endif()
    endforeach()
    set(${var} ${result} PARENT_SCOPE)
endfunction()
rebase_existing_sources(srcs ${srcs})
list(APPEND srcs
    ${ANGLE_ROOT}/src/common/base/anglebase/trace_event/trace_categories.cc
    ${ANGLE_ROOT}/src/libGLESv2/libGLESv2_autogen.cpp
    ${ANGLE_ROOT}/src/libEGL/egl_loader_autogen.cpp)
if(UNIX AND NOT APPLE)
    list(APPEND srcs ${ANGLE_ROOT}/src/common/linux/window_system.cpp)
endif()
list(REMOVE_DUPLICATES srcs)

if(BUILD_SWIFTSHADER_VULKAN)
    if(NOT EXISTS "${SWIFTSHADER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "SwiftShader source not found: ${SWIFTSHADER_SOURCE_DIR}")
    endif()
    set(SWIFTSHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/swiftshader-build" CACHE PATH "Out-of-tree SwiftShader build directory")
    set(SWIFTSHADER_CMAKE_ARGS -DSWIFTSHADER_BUILD_TESTS=OFF -DSWIFTSHADER_BUILD_BENCHMARKS=OFF -DREACTOR_BACKEND=LLVM)
    add_custom_target(swiftshader_vulkan ALL
        COMMAND ${CMAKE_COMMAND} -S "${SWIFTSHADER_SOURCE_DIR}" -B "${SWIFTSHADER_BUILD_DIR}" ${SWIFTSHADER_CMAKE_ARGS}
        # 单配置生成器（Ninja/Make）不需要 --config；多配置构建可自行追加。
        COMMAND ${CMAKE_COMMAND} --build "${SWIFTSHADER_BUILD_DIR}" --target vk_swiftshader
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SWIFTSHADER_DIST_DIR}"
        USES_TERMINAL)
    if(WIN32)
        set(SWIFTSHADER_LIBRARY_NAME "vk_swiftshader.dll")
        set(SWIFTSHADER_BUILD_LIBRARY "${SWIFTSHADER_BUILD_DIR}/$<CONFIG>/vk_swiftshader.dll")
    else()
        set(SWIFTSHADER_LIBRARY_NAME "libvk_swiftshader.so")
        set(SWIFTSHADER_BUILD_LIBRARY "${SWIFTSHADER_BUILD_DIR}/libvk_swiftshader.so")
    endif()
    set(SWIFTSHADER_ICD_FILE "${SWIFTSHADER_DIST_DIR}/vk_swiftshader_icd.json")
    # Generate the ICD manifest at configure time: CMake -E echo + shell
    # redirection strips the JSON quotes, producing an invalid manifest.
    # The loader does not resolve a relative library_path against the JSON
    # directory, so an absolute path is required.
    file(WRITE "${SWIFTSHADER_ICD_FILE}"
        "{\"file_format_version\":\"1.0.0\",\"ICD\":{\"library_path\":\"${SWIFTSHADER_DIST_DIR}/${SWIFTSHADER_LIBRARY_NAME}\",\"api_version\":\"1.0.5\"}}")
    add_custom_command(TARGET swiftshader_vulkan POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SWIFTSHADER_BUILD_LIBRARY}" "${SWIFTSHADER_DIST_DIR}/${SWIFTSHADER_LIBRARY_NAME}")
endif()
include_directories(
    ${ANGLE_ROOT}/src
    ${ANGLE_ROOT}/include
    ${ANGLE_ROOT}/src/common/third_party/base
    ${CMAKE_CURRENT_LIST_DIR}/angle_commit
    ${CMAKE_CURRENT_BINARY_DIR}/angle/includes)

if(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
    add_definitions(/wd4996) # FIXME:
endif()

if(WIN32)
    add_definitions(-DANGLE_IS_WIN)
endif()

if(MSVC)
    add_compile_options(/MP)
endif()

# 编译配置
# 该 ANGLE checkout 使用 C++20 特性（如 common/span.h 中的 concept）。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(
    -DANGLE_REVISION=9999
    -D_USE_MATH_DEFINES
    -DANGLE_TRANSLATOR_IMPLEMENTATION
    -DGL_GLEXT_PROTOTYPES
    -DEGL_EGLEXT_PROTOTYPES
    )

# Debugging
add_definitions(
    #-DANGLE_ENABLE_DEBUG_TRACE=1
    #-DANGLE_ENABLE_TRACE_EVENTS=1
    )

# MSVCRT speicfic?
add_definitions(-DNOMINMAX)

# 后端选择：按开关注入对应后端的编译宏
#   Vulkan: ANGLE_ENABLE_VULKAN + volk 动态加载（ANGLE_SHARED_LIBVULKAN）
#   OpenGL: ANGLE_ENABLE_OPENGL + 桌面 GL/GLES（在 4.源码 节收集源）
if(angle_enable_vk)
    add_definitions(-DANGLE_ENABLE_VULKAN -DANGLE_USE_CUSTOM_VULKAN_CMD_BUFFERS=1
        -DANGLE_ENABLE_VULKAN_GPU_TRACE_EVENTS=1)
    if(WIN32)
        add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    endif()
    if(NOT ANGLE_VOLK_ROOT)
        set(ANGLE_VOLK_ROOT "${ANGLE_ROOT}/src/third_party/volk")
    endif()
    include_directories(${ANGLE_VOLK_ROOT})
    list(APPEND srcs ${ANGLE_ROOT}/src/third_party/volk/volk.c)
    add_definitions(-DANGLE_SHARED_LIBVULKAN=1 -DVK_NO_PROTOTYPES)
    if(NOT EXISTS "${ANGLE_VMA_ROOT}/include/vk_mem_alloc.h")
        message(FATAL_ERROR "ANGLE_VMA_ROOT does not contain include/vk_mem_alloc.h")
    endif()
    list(APPEND srcs ${CMAKE_CURRENT_SOURCE_DIR}/vma_gen.cpp)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vma_include)
    include_directories(${ANGLE_VMA_ROOT}/include)
endif()
if(angle_enable_dx)
    add_definitions(-DANGLE_ENABLE_HLSL -DANGLE_ENABLE_D3D11)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(sixtyfour OFF)
else()
    set(sixtyfour ON)
endif()
if(sixtyfour)
    add_definitions(-DANGLE_IS_64_BIT_CPU)
else()
    add_definitions(-DANGLE_IS_32_BIT_CPU)
endif()

# FIXME: Correct?
add_definitions(
    "-DANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES={ \"d3dcompiler_47.dll\", \"d3dcompiler_46.dll\", \"d3dcompiler_43.dll\" }"
    )
add_definitions(
    "-DANGLE_VK_MOCK_ICD_JSON=\"\""
    "-DANGLE_VK_LAYERS_DIR=\"\""
    )

# Headers required for User
# Definitions required for User
add_definitions(
    # Use static-link
    -DANGLE_EXPORT=
    -DKHRONOS_STATIC
    #-DEGLAPI=
    #-DGL_API=
    #-DGL_APIENTRY=
    #-DGL_APICALL=

    # zlib
    -DCPU_NO_SIMD
    -DCHROMIUM_ZLIB_NO_CHROMECONF
    )

set(chromium_zlib_srcs
    ${CHROMIUM_ZLIB_ROOT}/adler32.c
    ${CHROMIUM_ZLIB_ROOT}/compress.c
    ${CHROMIUM_ZLIB_ROOT}/cpu_features.c
    ${CHROMIUM_ZLIB_ROOT}/crc32.c
    ${CHROMIUM_ZLIB_ROOT}/deflate.c
    ${CHROMIUM_ZLIB_ROOT}/gzclose.c
    ${CHROMIUM_ZLIB_ROOT}/gzlib.c
    ${CHROMIUM_ZLIB_ROOT}/gzread.c
    ${CHROMIUM_ZLIB_ROOT}/gzwrite.c
    ${CHROMIUM_ZLIB_ROOT}/infback.c
    ${CHROMIUM_ZLIB_ROOT}/inffast.c
    ${CHROMIUM_ZLIB_ROOT}/inflate.c
    ${CHROMIUM_ZLIB_ROOT}/inftrees.c
    ${CHROMIUM_ZLIB_ROOT}/trees.c
    ${CHROMIUM_ZLIB_ROOT}/uncompr.c
    ${CHROMIUM_ZLIB_ROOT}/zutil.c

    ${CHROMIUM_ZLIB_ROOT}/google/compression_utils_portable.cc
    )

include_directories(
    ${CHROMIUM_ZLIB_ROOT}
    ${CHROMIUM_ZLIB_ROOT}/google)

# The generated ANGLE entry points are part of the static library.  Keep the
# loader implementation here as well; this is required by the sample utility.
list(APPEND srcs
    ${ANGLE_ROOT}/src/libGLESv2/libGLESv2_autogen.cpp
    ${ANGLE_ROOT}/src/libEGL/egl_loader_autogen.cpp)

# 目标布局（对齐官方 GN）：
#   angle_glesv2_shared —— ANGLE 主体：EGL/GLES 全部实现 + 各后端 + 内嵌 zlib
#   angle_egl_shared   —— 仅 EGL 转发 ABI，链接 angle_glesv2_shared
#   angle_glesv1_shared —— GLES1 兼容层
# 官方 GN 布局：libGLESv2 拥有 ANGLE 与 GLES 入口；libEGL 只是转发库。
list(REMOVE_ITEM srcs ${angle_libegl_sources})
set(angle_glesv2_sources ${srcs} ${chromium_zlib_srcs}
    ${angle_libglesv2_sources}
    ${ANGLE_ROOT}/src/libGLESv2/libGLESv2_autogen.def)
rebase_existing_sources(angle_glesv2_sources ${angle_glesv2_sources})
# libEGL is the public EGL forwarding ABI. The EGL implementation itself is
# provided by the shared ANGLE core in libGLESv2; only the sample-side loader
# belongs in angle_sample_util, never inside libEGL.
set(angle_egl_sources
    ${ANGLE_ROOT}/src/libEGL/libEGL_autogen.cpp
    ${ANGLE_ROOT}/src/libEGL/libEGL_autogen.def
    ${ANGLE_ROOT}/src/common/system_utils.cpp
    ${ANGLE_ROOT}/src/common/debug.cpp
    ${ANGLE_ROOT}/src/common/angleutils.cpp
    ${ANGLE_ROOT}/src/common/entry_points_enum_autogen.cpp
    ${ANGLE_ROOT}/src/common/SimpleMutex.cpp)
if(UNIX AND NOT APPLE)
    list(APPEND angle_egl_sources
        ${ANGLE_ROOT}/src/common/system_utils_posix.cpp
        ${ANGLE_ROOT}/src/common/system_utils_linux.cpp)
endif()
# EGL forwarding target setup
set(angle_glesv2_exports_def "${CMAKE_CURRENT_BINARY_DIR}/libGLESv2_exports.def")
file(WRITE "${angle_glesv2_exports_def}" "LIBRARY libGLESv2\nEXPORTS\n")
file(STRINGS "${ANGLE_ROOT}/src/libGLESv2/libGLESv2_autogen.def" angle_public_exports)
foreach(angle_export IN LISTS angle_public_exports)
    if(angle_export MATCHES "^[ \t]*(gl[A-Za-z0-9_]+)([ \t].*)?$")
        file(APPEND "${angle_glesv2_exports_def}" "    ${CMAKE_MATCH_1}\n")
    elseif(angle_export MATCHES "^[ \t]*(EGL_[A-Za-z0-9_]+)([ \t].*)?$")
        file(APPEND "${angle_glesv2_exports_def}" "    ${CMAKE_MATCH_1}\n")
    endif()
endforeach()
# Export only the private GL_* symbols referenced by libGLESv1_CM.cpp.
file(STRINGS "${ANGLE_ROOT}/src/libGLESv1_CM/libGLESv1_CM.cpp" angle_v1_lines REGEX "GL_[A-Za-z0-9_]+")
foreach(angle_line IN LISTS angle_v1_lines)
    string(REGEX MATCHALL "GL_[A-Za-z0-9_]+" angle_v1_names "${angle_line}")
    foreach(angle_internal_name IN LISTS angle_v1_names)
        if(NOT angle_internal_name STREQUAL "GL_APIENTRY" AND NOT angle_internal_name MATCHES "^GL_(OES|EXT|ANGLE|KHR|WEBGL|NV|AMD|IMG|CHROMIUM|APPLE|ARM|QCOM|MESA|OVR|INTEL|FJ|DMP|VIVANTE|MOZILLA)_")
            file(APPEND "${angle_glesv2_exports_def}" "    ${angle_internal_name}\n")
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES angle_glesv2_sources)
list(REMOVE_ITEM angle_glesv2_sources ${ANGLE_ROOT}/src/libGLESv2/libGLESv2_autogen.def)
list(APPEND angle_glesv2_sources "${angle_glesv2_exports_def}")
add_library(angle_glesv2_shared SHARED ${angle_glesv2_sources})
target_compile_definitions(angle_glesv2_shared PRIVATE LIBGLESV2_IMPLEMENTATION LIBANGLE_IMPLEMENTATION ANGLE_EXPORT=)
target_include_directories(angle_glesv2_shared PUBLIC ${ANGLE_ROOT}/include ${ANGLE_ROOT}/src ${CMAKE_CURRENT_SOURCE_DIR}/angle_commit)
# Bind the shared library's internal function calls (its bundled zlib etc.)
# to its own definitions; otherwise symbol interposition from the host
# executable's static zlib breaks gzip decompression inside ANGLE.
if(UNIX AND NOT APPLE)
    target_link_options(angle_glesv2_shared PRIVATE -Wl,-Bsymbolic-functions)
endif()
# GCC 16 rejects a StrictNumeric conversion in IntermNode.cpp that older
# compilers accept; vendor source cannot be modified, so downgrade the
# diagnostics (only for the ANGLE translation unit set).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    target_compile_options(angle_glesv2_shared PRIVATE -fpermissive)
endif()
if(WIN32)
    target_link_libraries(angle_glesv2_shared PRIVATE d3d11 dxgi d3dcompiler dxguid user32 gdi32 advapi32 shell32 ole32 uuid imm32 version synchronization)
    if(angle_enable_vk)
        target_link_libraries(angle_glesv2_shared PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/build-swiftshader/third_party/SPIRV-Tools/source/Release/SPIRV-Tools.lib" "${CMAKE_CURRENT_SOURCE_DIR}/build-swiftshader/third_party/SPIRV-Tools/source/opt/Release/SPIRV-Tools-opt.lib")
    endif()
endif()
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    # Linux 链接：GLX 后端需要 X11/Xi；EGL 后端运行时加载系统 EGL。
    target_link_libraries(angle_glesv2_shared PRIVATE Threads::Threads ${CMAKE_DL_LIBS} X11 Xi)
    if(angle_enable_vk)
        # Vulkan backend uses Wayland/GBM platform sources and SPIRV-Tools.
        target_link_libraries(angle_glesv2_shared PRIVATE wayland-client gbm)
        if(NOT TARGET SPIRV-Tools)
            set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/third_party/SPIRV-Headers")
            add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/swiftshader/third_party/SPIRV-Tools"
                             "${CMAKE_CURRENT_BINARY_DIR}/spirv-tools" EXCLUDE_FROM_ALL)
        endif()
        target_link_libraries(angle_glesv2_shared PRIVATE SPIRV-Tools SPIRV-Tools-opt)
    endif()
endif()

add_library(angle_egl_shared SHARED ${angle_egl_sources})
target_compile_definitions(angle_egl_shared PRIVATE LIBEGL_IMPLEMENTATION EGLAPI=)
target_include_directories(angle_egl_shared PUBLIC ${ANGLE_ROOT}/include ${ANGLE_ROOT}/src ${CMAKE_CURRENT_SOURCE_DIR}/angle_commit)
target_link_libraries(angle_egl_shared PRIVATE angle_glesv2_shared)
if(WIN32)
    target_link_libraries(angle_egl_shared PRIVATE synchronization)
elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(angle_egl_shared PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()
set_target_properties(angle_egl_shared PROPERTIES PREFIX "" OUTPUT_NAME libEGL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(angle_glesv1_shared SHARED
    ${ANGLE_ROOT}/src/libGLESv1_CM/libGLESv1_CM.cpp
    ${ANGLE_ROOT}/src/libGLESv1_CM/resource.h
    ${ANGLE_ROOT}/src/libGLESv1_CM/libGLESv1_CM.def)
target_compile_definitions(angle_glesv1_shared PRIVATE LIBGLESV1_CM_IMPLEMENTATION ANGLE_EXPORT=)
target_include_directories(angle_glesv1_shared PUBLIC ${ANGLE_ROOT}/include ${ANGLE_ROOT}/src)
if(UNIX AND NOT APPLE)
    target_link_libraries(angle_glesv1_shared PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
elseif(WIN32)
    target_link_libraries(angle_glesv1_shared PRIVATE synchronization)
endif()
set_target_properties(angle_glesv1_shared PROPERTIES PREFIX "" OUTPUT_NAME libGLESv1_CM RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(angle_static STATIC ${srcs}
    ${chromium_zlib_srcs})

target_compile_definitions(angle_static PUBLIC ANGLE_USE_EGL_LOADER
    ANGLE_DISPATCH_LIBRARY="libGLESv2" )
target_include_directories(angle_static PUBLIC
    ${ANGLE_ROOT}/include
    ${ANGLE_ROOT}/src
    ${ANGLE_ROOT}/src/common/base
    ${ANGLE_ROOT}/util)

if(WIN32)
    target_compile_definitions(angle_static PUBLIC ANGLE_PLATFORM_WINDOWS)
    target_link_libraries(angle_static PUBLIC d3d11 dxgi d3dcompiler dxguid
        user32 gdi32 advapi32 shell32 ole32 uuid imm32 version synchronization)
endif()

# A small, self-contained sample utility target mirrors the sources used by
# ANGLE's official samples, without pulling tests or the GN build system in.
set(angle_sample_util_sources
    ${ANGLE_ROOT}/samples/sample_util/SampleApplication.cpp
    ${ANGLE_ROOT}/util/EGLWindow.cpp
    ${ANGLE_ROOT}/util/OSWindow.cpp
    ${ANGLE_ROOT}/util/Matrix.cpp
    ${ANGLE_ROOT}/util/Timer.cpp
    ${ANGLE_ROOT}/util/geometry_utils.cpp
    ${ANGLE_ROOT}/util/random_utils.cpp
    ${ANGLE_ROOT}/util/shader_utils.cpp
    ${ANGLE_ROOT}/util/test_utils.cpp
    ${ANGLE_ROOT}/util/windows/test_utils_win.cpp
    ${ANGLE_ROOT}/util/autogen/angle_features_autogen.cpp
    ${ANGLE_ROOT}/util/egl_loader_autogen.cpp
    ${ANGLE_ROOT}/util/gles_loader_autogen.cpp)
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    list(APPEND angle_sample_util_sources
        ${ANGLE_ROOT}/util/windows/win32/Win32Window.cpp
        ${ANGLE_ROOT}/util/windows/win32/Win32Pixmap.cpp
        ${ANGLE_ROOT}/util/windows/WGLWindow.cpp
        ${ANGLE_ROOT}/util/windows/wgl_loader_autogen.cpp
        ${ANGLE_ROOT}/src/common/system_utils.cpp
        ${ANGLE_ROOT}/src/common/system_utils_win.cpp
        ${ANGLE_ROOT}/src/common/system_utils_win32.cpp
        ${ANGLE_ROOT}/src/common/debug.cpp
        ${ANGLE_ROOT}/src/common/angleutils.cpp
        ${ANGLE_ROOT}/src/common/utilities.cpp
        ${ANGLE_ROOT}/src/common/string_utils.cpp
        ${ANGLE_ROOT}/src/common/platform_helpers.cpp
        ${ANGLE_ROOT}/src/common/entry_points_enum_autogen.cpp
        ${ANGLE_ROOT}/src/common/gl_enum_utils.cpp
        ${ANGLE_ROOT}/src/common/gl_enum_utils_autogen.cpp
        ${ANGLE_ROOT}/src/common/SimpleMutex.cpp)
endif()
add_library(angle_sample_util STATIC ${angle_sample_util_sources})
target_compile_definitions(angle_sample_util PUBLIC ANGLE_USE_UTIL_LOADER
    LIBANGLE_UTIL_IMPLEMENTATION ANGLE_PLATFORM_WINDOWS GL_GLES_PROTOTYPES=0 ANGLE_EGL_LIBRARY_NAME="libEGL")
target_include_directories(angle_sample_util PUBLIC
    ${ANGLE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ANGLE_ROOT}/include ${ANGLE_ROOT}/src
    ${ANGLE_ROOT}/samples/sample_util)
target_link_libraries(angle_sample_util PUBLIC angle_egl_shared angle_glesv2_shared)

add_executable(angle_color_triangle
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/color_triangle/ColorTriangle.cpp)
target_include_directories(angle_color_triangle PRIVATE
    ${ANGLE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ANGLE_ROOT}/include ${ANGLE_ROOT}/src
    ${ANGLE_ROOT}/samples/sample_util)
target_compile_definitions(angle_color_triangle PRIVATE ANGLE_USE_UTIL_LOADER
    LIBANGLE_UTIL_IMPLEMENTATION ANGLE_PLATFORM_WINDOWS GL_GLES_PROTOTYPES=0 ANGLE_EGL_LIBRARY_NAME="libEGL")
target_link_libraries(angle_color_triangle PRIVATE angle_sample_util synchronization)
if(WIN32)
    target_link_libraries(angle_color_triangle PRIVATE user32 gdi32)
endif()

if(WIN32)
    add_custom_command(TARGET angle_color_triangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:angle_egl_shared> $<TARGET_FILE_DIR:angle_color_triangle>/libEGL.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:angle_glesv2_shared> $<TARGET_FILE_DIR:angle_color_triangle>/libGLESv2.dll)
endif()

set_target_properties(angle_color_triangle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(angle_color_triangle_shared
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/color_triangle/ColorTriangle.cpp)
target_include_directories(angle_color_triangle_shared PRIVATE ${ANGLE_ROOT} ${CMAKE_CURRENT_SOURCE_DIR} ${ANGLE_ROOT}/include ${ANGLE_ROOT}/src ${ANGLE_ROOT}/samples/sample_util)
target_compile_definitions(angle_color_triangle_shared PRIVATE ANGLE_USE_UTIL_LOADER LIBANGLE_UTIL_IMPLEMENTATION GL_GLES_PROTOTYPES=0 ANGLE_EGL_LIBRARY_NAME="libEGL")
target_link_libraries(angle_color_triangle_shared PRIVATE angle_sample_util angle_glesv2_shared synchronization)
add_dependencies(angle_color_triangle_shared angle_egl_shared angle_glesv2_shared)
set_target_properties(angle_color_triangle_shared PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET angle_color_triangle_shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:angle_egl_shared> $<TARGET_FILE_DIR:angle_color_triangle_shared>/libEGL.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:angle_glesv2_shared> $<TARGET_FILE_DIR:angle_color_triangle_shared>/libGLESv2.dll)

