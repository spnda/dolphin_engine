#pragma once

#ifdef WITH_NV_AFTERMATH

#include <vector>

#include <vulkan/vulkan.h> // Needed for vulkan-specific definitions
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"

namespace dp::ShaderDatabase {
    void addShaderBinary(std::vector<uint32_t>& binary);
    void addShaderDebugInfos(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier, std::vector<uint8_t>& debugInfos);
    void addShaderWithDebugInfo(std::vector<uint32_t>& strippedBinary, std::vector<uint32_t>& binary);
    bool findShaderBinary(const GFSDK_Aftermath_ShaderHash* shaderHash, std::vector<uint32_t>& binary);
    bool findShaderDebugInfos(const GFSDK_Aftermath_ShaderDebugInfoIdentifier* identifier, std::vector<uint8_t>& debugInfos);
    bool findShaderBinaryWithDebugInfo(const GFSDK_Aftermath_ShaderDebugName* shaderDebugName, std::vector<uint32_t>& binary);
}

#endif // #ifdef WITH_NV_AFTERMATH
