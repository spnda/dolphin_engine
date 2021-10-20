#pragma once

namespace dp {
    struct EngineOptions final {
        const std::vector<const char*> scenes = {
            "models/CornellBox/CornellBox-Original.obj",
            "models/SunTemple/SunTemple.fbx",
            "models/Sponza/sponza.obj",
            "models/San Miguel/san-miguel.obj",
            "models/Sibenik/sibenik.obj"
        };

        uint32_t sceneIndex = 0;
    };
}
