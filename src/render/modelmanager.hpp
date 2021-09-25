#pragma once

#include <thread>
#include <mutex>

#include "mesh.hpp"
#include "../vulkan/rt/acceleration_structure.hpp"

namespace dp {
    class Context;
    class ModelLoader;

    /** Manages all models and acceleration structures */
    class ModelManager {
        const uint32_t threadCount = std::thread::hardware_concurrency();
        const dp::Context& ctx;

        std::mutex tlasMutex;
        dp::AccelerationStructure topLevelAccelerationStructure;
        dp::Buffer materialBuffer;

        /** A vector of all materials for all meshes */
        std::vector<Material> materials = {{}};
        uint32_t currentMaterialOffset;
        std::vector<Mesh> meshes = {};

        std::vector<std::thread> currentThreads = {};

        /** Mesh loading function executed on a single thread */
        void loadMesh(std::string fileName);
        void applyMaterialOffsets(std::vector<Mesh>& newMaterials);
        void updateTLAS(const dp::ModelLoader& modelLoader);

    public:
        ModelManager(const dp::Context& context);

        /** Builds an empty acceleration structure to begin with */
        void buildTopLevelAccelerationStructure();

        void loadMeshes(std::initializer_list<std::string> files);

        dp::AccelerationStructure& getTLAS();
        dp::Buffer& getMaterialBuffer();
    };
}
