"""Prepare a tracked, pinned DXVK Native tree with marked Android changes.

The portability requirements were compared with the NFSU2 reference port.
Only upstream DXVK/Khronos source is copied; the WSI adapter is local code.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

PIN = "c8dc91fabd00cac11d697ccf07426e798393cd40"
HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(source: Path, output: Path, work: Path):
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if revision != PIN:
        raise ValueError("Unexpected DXVK revision: " + revision)
    dirty = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain", "--untracked-files=all"], text=True)
    if dirty.strip():
        raise ValueError("DXVK checkout must be clean")
    recipe = digest(Path(__file__))
    supplemental = digest(HERE / "include/vulkan_android.h")
    identity = {"revision": PIN, "recipe_sha256": recipe, "android_header_sha256": supplemental}
    stamp = output / "android-preparation.json"
    if stamp.exists():
        previous = json.loads(stamp.read_text(encoding="utf-8"))
        if all(previous.get(k) == v for k, v in identity.items()):
            actual = {name: digest(output / name) for name in previous["files"] if (output / name).is_file()}
            if actual == previous["files"]:
                return previous
    # Restrict recursive removal to this builder's one generated source tree.
    if output.resolve().parent != work.resolve() or output.name != "dxvk-source":
        raise ValueError("Prepared source must be work/dxvk-source")
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    tracked = subprocess.check_output(["git", "-C", str(source), "ls-files", "src", "include", "LICENSE"], text=True).splitlines()
    for name in tracked:
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, destination)
    shutil.copy2(HERE / "include/vulkan_android.h", output / "include/vulkan/vulkan_android.h")

    changes = []

    def patch(name, before, after):
        file = output / name
        text = file.read_text(encoding="utf-8")
        if text.count(before) != 1:
            raise ValueError("Patch expected one occurrence: " + name + " / " + before[:70])
        file.write_text(text.replace(before, after), encoding="utf-8", newline="\n")
        changes.append({"file": name, "before_sha256": hashlib.sha256(before.encode()).hexdigest(),
                        "after_sha256": hashlib.sha256(after.encode()).hexdigest()})

    patch("src/util/util_bit.h", "#pragma once", "#pragma once\n#include <cstdint>\n// Modified for Android portability by this project.")
    patch("src/util/util_bit.h", "#ifndef _MSC_VER", "#if defined(__i386__) || defined(__x86_64__)\n#ifndef _MSC_VER")
    patch("src/util/util_bit.h", '#include "util_likely.h"', '#endif // x86 intrinsics\n#include "util_likely.h"')
    patch("src/util/util_bit.h", "#elif defined(__GNUC__) || defined(__clang__)\n    uint32_t res;",
          "#elif defined(__aarch64__) || defined(__arm__)\n    return n ? __builtin_ctz(n) : 32;\n    #elif defined(__GNUC__) || defined(__clang__)\n    uint32_t res;")
    patch("src/util/util_bit.h", "#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)", "#if defined(__i386__) || defined(__x86_64__)")
    patch("src/util/sync/sync_spinlock.h", "_mm_pause();", '#if defined(__aarch64__) || defined(__arm__)\n        asm volatile("yield");\n#else\n        _mm_pause();\n#endif')
    patch("src/util/util_vector.h", "    __m128 value = _mm_load_ps(a.data);", "#if defined(__aarch64__) || defined(__arm__)\n    for (unsigned i = 0; i < 4; ++i) result.data[i] = a.data[i] == a.data[i] ? a.data[i] : 0.0f;\n#else\n    __m128 value = _mm_load_ps(a.data);")
    patch("src/util/util_vector.h", "    _mm_store_ps(result.data, value);", "    _mm_store_ps(result.data, value);\n#endif")
    # DXVK's native readlink code must not construct an invalid range on failure.
    patch("src/util/platform/util_env_linux.cpp", '    size_t count = readlink("/proc/self/exe", exePath.data(), exePath.size());',
          '    ssize_t count = readlink("/proc/self/exe", exePath.data(), exePath.size());\n    if (count < 0) return {};')
    for feature in ("textureCompressionBC", "multiViewport", "geometryShader", "sampleRateShading", "depthClamp", "fillModeNonSolid", "shaderClipDistance", "shaderCullDistance"):
        patch("src/d3d9/d3d9_device.cpp", f"    enabled.core.features.{feature} = VK_TRUE;",
              f"    enabled.core.features.{feature} = supported.core.features.{feature};")
    # Vertex shader writers must follow the feature requested at device creation.
    patch("src/d3d9/d3d9_device.cpp", "namespace dxvk {\n", "namespace dxvk {\n\n  bool g_androidClipDistance = true;\n")
    patch("src/d3d9/d3d9_device.cpp", "    enabled.core.features.shaderClipDistance = supported.core.features.shaderClipDistance;",
          "    enabled.core.features.shaderClipDistance = supported.core.features.shaderClipDistance;\n    g_androidClipDistance = supported.core.features.shaderClipDistance;")
    patch("src/dxso/dxso_compiler.cpp", "namespace dxvk {\n", "namespace dxvk {\n  extern bool g_androidClipDistance;\n")
    patch("src/dxso/dxso_compiler.cpp", "    m_module.enableCapability(spv::CapabilityClipDistance);", "    if (g_androidClipDistance) m_module.enableCapability(spv::CapabilityClipDistance);")
    patch("src/dxso/dxso_compiler.cpp", "    this->emitVsClipping();", "    if (g_androidClipDistance) this->emitVsClipping();")
    patch("src/d3d9/d3d9_fixed_function.cpp", "namespace dxvk {\n", "namespace dxvk {\n  extern bool g_androidClipDistance;\n")
    patch("src/d3d9/d3d9_fixed_function.cpp", "    m_module.enableCapability(spv::CapabilityClipDistance);", "    if (g_androidClipDistance) m_module.enableCapability(spv::CapabilityClipDistance);")
    patch("src/d3d9/d3d9_fixed_function.cpp", "    if (m_vsKey.Data.Contents.VertexClipping)", "    if (m_vsKey.Data.Contents.VertexClipping && g_androidClipDistance)")
    patch("src/d3d9/d3d9_device.cpp", "    state.polygonMode     = DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE]));",
          "    state.polygonMode     = DecodeFillMode(D3DFILLMODE(rs[D3DRS_FILLMODE]));\n    if (!m_dxvkDevice->features().core.features.fillModeNonSolid) state.polygonMode = VK_POLYGON_MODE_FILL;")
    # No unsupported depthClamp feature may reach VkPipelineRasterizationState.
    patch("src/d3d9/d3d9_device.cpp", "    enabled.core.features.depthClamp = supported.core.features.depthClamp;",
          "    enabled.core.features.depthClamp = supported.core.features.depthClamp;\n    g_androidDepthClamp = supported.core.features.depthClamp;")
    patch("src/d3d9/d3d9_device.cpp", "  bool g_androidClipDistance = true;", "  bool g_androidClipDistance = true;\n  extern bool g_androidDepthClamp;")
    patch("src/dxvk/dxvk_graphics.cpp", "namespace dxvk {\n", "namespace dxvk {\n  bool g_androidDepthClamp = true;\n")
    patch("src/dxvk/dxvk_graphics.cpp", "    rsInfo.depthClampEnable       = VK_TRUE;", "    rsInfo.depthClampEnable       = g_androidDepthClamp;")
    patch("src/dxvk/dxvk_graphics.cpp", "      rsInfo.depthClampEnable = !state.rs.depthClipEnable();", "      rsInfo.depthClampEnable = g_androidDepthClamp && !state.rs.depthClipEnable();")
    for meta in ("blit", "copy", "resolve"):
        name = f"src/dxvk/dxvk_meta_{meta}.cpp"
        patch(name, "namespace dxvk {\n", "namespace dxvk {\n  extern bool g_androidDepthClamp;\n")
        patch(name, "    rsState.depthClampEnable    = VK_TRUE;", "    rsState.depthClampEnable    = g_androidDepthClamp;")
    patch("src/d3d9/d3d9_swapchain.cpp", "    if (status != VK_SUCCESS)\n      RecreateSwapChain(m_vsync);", "    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)\n      RecreateSwapChain(m_vsync);")
    patch("src/d3d9/d3d9_swapchain.cpp",
          "      VkRect2D dstRect = {\n        {  int32_t(m_dstRect.left),                    int32_t(m_dstRect.top)                    },\n        { uint32_t(m_dstRect.right - m_dstRect.left), uint32_t(m_dstRect.bottom - m_dstRect.top) } };",
          "      VkRect2D dstRect = {\n        {  int32_t(m_dstRect.left),                    int32_t(m_dstRect.top)                    },\n        { uint32_t(m_dstRect.right - m_dstRect.left), uint32_t(m_dstRect.bottom - m_dstRect.top) } };\n\n"
          "#if defined(DXVK_WSI_ANDROID)\n"
          "      // Android fullscreen must scale a reduced D3D9 backbuffer to the\n"
          "      // physical swapchain extent, even if the guest requested a small window.\n"
          "      dstRect.offset = { 0, 0 };\n"
          "      dstRect.extent = info.imageExtent;\n"
          "#endif")
    patch("src/d3d9/d3d9_swapchain.cpp", "    for (auto& backBuffer : m_backBuffers)\n      backBuffer->ClearContainer();", "    for (auto& backBuffer : m_backBuffers)\n      if (backBuffer != nullptr) backBuffer->ClearContainer();")
    # Actual-device validation: Android surfaces may support INHERIT only.
    patch("src/vulkan/vulkan_presenter.cpp", "    swapInfo.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;",
          "    if (!caps.supportedCompositeAlpha) return VK_ERROR_INITIALIZATION_FAILED;\n"
          "    swapInfo.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(\n"
          "      (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)\n"
          "        ? uint32_t(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)\n"
          "        : (caps.supportedCompositeAlpha & (~caps.supportedCompositeAlpha + 1u)));")
    # The zero-swizzled sampled dummy must permit linear filtering. R32_SFLOAT
    # does not on Adreno 650. RGBA8_UNORM is in the same 32-bit compatibility
    # class as the mutable R32_UINT backing image; all sampled channels stay 0.
    patch("src/dxvk/dxvk_unbound.cpp", "m_viewsSampled  (createImageViews(dev, VK_FORMAT_R32_SFLOAT))",
          "m_viewsSampled  (createImageViews(dev, VK_FORMAT_R8G8B8A8_UNORM))")
    # Preserve conservative synchronization with identical dependencies for
    # every load/store variant, including the pipeline/framebuffer default.
    # Varying dependencyCount violated render-pass compatibility on device.
    patch("src/dxvk/dxvk_renderpass.cpp", "    VkRenderPassCreateInfo info;",
          "    subpassDepCount = 3;\n"
          "    subpassDeps[0] = { VK_SUBPASS_EXTERNAL, 0,\n"
          "      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,\n"
          "      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,\n"
          "      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0 };\n"
          "    subpassDeps[1] = { 0, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,\n"
          "      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,\n"
          "      VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT };\n"
          "    subpassDeps[2] = { 0, VK_SUBPASS_EXTERNAL,\n"
          "      VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,\n"
          "      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,\n"
          "      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0 };\n\n"
          "    VkRenderPassCreateInfo info;")
    # Legacy DXSO shaders alias 2D/3D/cube descriptors behind specialization
    # constants. Freeze the actual pipeline values and eliminate dead branches
    # with the NDK's SPIRV-Tools before the mobile driver sees the module.
    patch("src/dxvk/dxvk_shader.h", "    uint32_t  undefinedInputs = 0;",
          "    uint32_t  undefinedInputs = 0;\n    const VkSpecializationInfo* specialization = nullptr;")
    patch("src/dxvk/dxvk_shader.h", "      const SpirvCodeBuffer&      code);",
          "      const SpirvCodeBuffer&      code,\n      const VkSpecializationInfo* specialization = nullptr);")
    patch("src/dxvk/dxvk_shader.cpp", '#include "dxvk_shader.h"',
          '#include "dxvk_shader.h"\n#include "spirv_specialize.h"')
    patch("src/dxvk/dxvk_shader.cpp", "    const SpirvCodeBuffer&      code)",
          "    const SpirvCodeBuffer&      code,\n    const VkSpecializationInfo* specialization)")
    patch("src/dxvk/dxvk_shader.cpp", "    info.pCode    = code.data();",
          "    info.pCode    = code.data();\n    std::vector<uint32_t> specialized;\n"
          "    if (specialization && shader->stage() == VK_SHADER_STAGE_FRAGMENT_BIT) {\n"
          "      if (!tfSpirvSpecialize(code.data(), code.size() / sizeof(uint32_t), specialization, specialized))\n"
          '        throw DxvkError("Android SPIR-V specialization failed");\n'
          "      info.codeSize = specialized.size() * sizeof(uint32_t);\n"
          "      info.pCode = specialized.data();\n    }")
    patch("src/dxvk/dxvk_shader.cpp", "    return DxvkShaderModule(vkd, this, spirvCode);",
          "    return DxvkShaderModule(vkd, this, spirvCode, info.specialization);")
    patch("src/dxvk/dxvk_graphics.h", "    DxvkShaderModule createShaderModule(\n      const Rc<DxvkShader>&                shader,\n      const DxvkGraphicsPipelineStateInfo& state) const;",
          "    DxvkShaderModule createShaderModule(\n      const Rc<DxvkShader>&                shader,\n      const DxvkGraphicsPipelineStateInfo& state,\n      const VkSpecializationInfo* specialization) const;")
    for stage in ("vs", "tcs", "tes", "gs", "fs"):
        patch("src/dxvk/dxvk_graphics.cpp", f"createShaderModule(m_shaders.{stage}, " + (" state);" if len(stage)==2 else "state);"),
              f"createShaderModule(m_shaders.{stage}, state, &specInfo);")
    patch("src/dxvk/dxvk_graphics.cpp", "  DxvkShaderModule DxvkGraphicsPipeline::createShaderModule(\n    const Rc<DxvkShader>&                shader,\n    const DxvkGraphicsPipelineStateInfo& state) const {",
          "  DxvkShaderModule DxvkGraphicsPipeline::createShaderModule(\n    const Rc<DxvkShader>&                shader,\n    const DxvkGraphicsPipelineStateInfo& state,\n    const VkSpecializationInfo* specialization) const {")
    patch("src/dxvk/dxvk_graphics.cpp", "    return shader->createShaderModule(m_vkd, m_slotMapping, info);",
          "    info.specialization = specialization;\n    return shader->createShaderModule(m_vkd, m_slotMapping, info);")
    patch("src/util/log/log.cpp", '#include "log.h"', '#include "log.h"\n#include <android/log.h>')
    patch("src/util/log/log.cpp", "        std::cerr << prefix << line << std::endl;",
          '        std::cerr << prefix << line << std::endl;\n        if (level >= LogLevel::Warn) __android_log_print(level >= LogLevel::Error ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN, "DXVK", "%s", line.c_str());')
    (output / "version.h").write_text('#define DXVK_VERSION "native-1.9.2b-transformers-android"\n', encoding="ascii")
    (output / "PINNED_REVISION").write_text(PIN + "\n", encoding="ascii")
    files = {str(p.relative_to(output)).replace("\\", "/"): digest(p) for p in sorted(output.rglob("*")) if p.is_file()}
    record = {**identity, "files": files, "patches": changes, "modified_source": True}
    stamp.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record
