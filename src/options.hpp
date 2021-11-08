#pragma once

namespace dp {
    struct EngineOptions final {
        const std::vector<const char*> scenes = {
            "models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf",
            "models/glTF-Sample-Models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf"
        };

        uint32_t sceneIndex = 0;
    };
}
