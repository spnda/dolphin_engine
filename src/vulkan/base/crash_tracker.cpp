#ifdef WITH_NV_AFTERMATH

#include "crash_tracker.hpp"

#include <fstream>

#include <fmt/core.h>

#include "../context.hpp"

// Static callback functions.
[[maybe_unused]] void gpuCrashDumpCallback(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData) {
    auto crashTracker = reinterpret_cast<dp::GpuCrashTracker*>(pUserData);
    crashTracker->onCrashDump(pGpuCrashDump, gpuCrashDumpSize);
}

[[maybe_unused]] void shaderDebugInfoCallback(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize,
                                              void* pUserData) {
    auto crashTracker = reinterpret_cast<dp::GpuCrashTracker*>(pUserData);
    crashTracker->onShaderDebugInfo(pShaderDebugInfo, shaderDebugInfoSize);
}

[[maybe_unused]] void crashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription,
                                                   void* pUserData) {
    auto crashTracker = reinterpret_cast<dp::GpuCrashTracker*>(pUserData);
    crashTracker->onDescription(addDescription);
}

dp::GpuCrashTracker::GpuCrashTracker(const Context& context) : ctx(context) {}

dp::GpuCrashTracker::GpuCrashTracker(const GpuCrashTracker& c) : ctx(c.ctx) {}

void dp::GpuCrashTracker::enable() {
    // Enable crash dumps
    checkAftermathError(GFSDK_Aftermath_EnableGpuCrashDumps(
        GFSDK_Aftermath_Version_API,
        GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
        GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
        gpuCrashDumpCallback,
        shaderDebugInfoCallback,
        crashDumpDescriptionCallback,
        this
    ));
}

void dp::GpuCrashTracker::disable() const {
    GFSDK_Aftermath_DisableGpuCrashDumps();
}

void dp::GpuCrashTracker::onCrashDump(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize) {
    std::lock_guard<std::mutex> lock(crashMutex);

    GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
    auto result = GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
        GFSDK_Aftermath_Version_API,
        pGpuCrashDump,
        gpuCrashDumpSize,
        &decoder
    );
    if (!checkAftermathError(result, "Failed to create crash dump decoder")) return;

    // Query shader information
    uint32_t shaderCount = 0;
    result = GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfoCount(decoder, &shaderCount);

    if (checkAftermathError(result)) {
        std::vector<GFSDK_Aftermath_GpuCrashDump_ShaderInfo> shaderInfos(shaderCount);
        result = GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfo(decoder, shaderCount, shaderInfos.data());

        if (checkAftermathError(result)) {
            for (const auto& shaderInfo : shaderInfos) {
                fmt::print("Active shader: hash = {:#x}, instance = {:#x}, type = {}\n",
                    shaderInfo.shaderHash, shaderInfo.shaderInstance, static_cast<uint32_t>(shaderInfo.shaderType));
            }
        }
    }

    // Query GPU page fault information.
    GFSDK_Aftermath_GpuCrashDump_PageFaultInfo pageFaultInfo = {};
    result = GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &pageFaultInfo);

    if (checkAftermathError(result)) {
        // Print information about the GPU page fault.
        fmt::print(stderr, "GPU page fault at {:#x}\n", pageFaultInfo.faultingGpuVA);

        if (pageFaultInfo.bHasResourceInfo) {
            fmt::print("Fault in resource starting at {:#x}\n", pageFaultInfo.resourceInfo.gpuVa);
            fmt::print("Size of resource: ({}, {}, {}, {}) = {} bytes\n",
                            pageFaultInfo.resourceInfo.width,
                            pageFaultInfo.resourceInfo.height,
                            pageFaultInfo.resourceInfo.depth,
                            pageFaultInfo.resourceInfo.mipLevels,
                            pageFaultInfo.resourceInfo.size);
            fmt::print("Format of resource: {}\n", pageFaultInfo.resourceInfo.format);
            fmt::print("Resource was destroyed: {}\n", pageFaultInfo.resourceInfo.bWasDestroyed);
        }
    }

    // Generate JSON for general crash dump information
    const uint32_t jsonDecoderFlags = GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO | GFSDK_Aftermath_GpuCrashDumpDecoderFlags_SHADER_INFO ; // Simple & quick
    uint32_t jsonSize = 0;
    result = GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
        decoder, jsonDecoderFlags, GFSDK_Aftermath_GpuCrashDumpFormatterFlags_UTF8_OUTPUT,
        nullptr, nullptr, nullptr, nullptr,
        this, &jsonSize);

    if (checkAftermathError(result)) {
        std::vector<char> json(jsonSize);
        result = GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder, json.size(), json.data());

        if (checkAftermathError(result)) {
            std::ofstream file("crashdump.json", std::ios::out);
            file << std::string(json.data());
            file.close();
        }
    }

    checkAftermathError(GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder), "Failed to destroy crash dump decoder");
}

void dp::GpuCrashTracker::onShaderDebugInfo(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize) {
    std::lock_guard<std::mutex> lock(crashMutex);

    GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
    auto result = GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
        GFSDK_Aftermath_Version_API,
        pShaderDebugInfo,
        shaderDebugInfoSize,
        &identifier
    );
    if (!checkAftermathError(result, "Failed to get shader debug info")) return;
}

void dp::GpuCrashTracker::onDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription) {
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, ctx.applicationName.c_str());
    std::string version = fmt::format("v{}.{}.{}",
                                      VK_API_VERSION_MAJOR(ctx.appVersion),
                                      VK_API_VERSION_MINOR(ctx.appVersion),
                                      VK_API_VERSION_PATCH(ctx.appVersion));
    addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, version.c_str());
}

bool dp::GpuCrashTracker::checkAftermathError(GFSDK_Aftermath_Result result, const std::string& message) {
    if (result != GFSDK_Aftermath_Result_Success) {
        if (!message.empty()) {
            fmt::print(stderr, "{}: {}\n", message, aftermathResultStrings.at(result));
        }
        return false;
    }
    return true;
}

#endif // #ifdef WITH_NV_AFTERMATH
