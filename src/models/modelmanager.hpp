#pragma once

#include <future>

#include "../vulkan/rt/acceleration_structure.hpp"
#include "fileloader.hpp"
#include "mesh.hpp"

namespace dp {
    class Engine;

    class ModelManager {
        const dp::Context& ctx;
        dp::Engine& engine;

        dp::FileLoader fileLoader;
        std::atomic<bool> sceneLoadFinished = false;
        std::thread fileLoadThread;

        std::vector<dp::Texture> textures;

        VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR, };

        void uploadTexture(dp::TextureFile& textureFile);

    public:
        std::vector<dp::InstanceDescription> instanceDescriptions;
        std::vector<dp::BottomLevelAccelerationStructure> blases;
        dp::TopLevelAccelerationStructure tlas;

        /** A buffer of all materials. */
        dp::Buffer materialBuffer;

        /** A buffer containing instanceDescriptions of BLASes */
        dp::Buffer instanceDescriptionBuffer;

        explicit ModelManager(const dp::Context& context, dp::Engine& engine);

        void createDescriptionBuffers();
        void buildBlases();
        /** Builds, or rebuilds the TLAS. Will invalid the handle, so call dp::Engine::updateTlas() right after. */
        void buildTlas();
        void destroy();
        /** First init call, creating a basic TLAS and a basic empty image. */
        void init();
        auto getTextureDescriptorInfos() -> std::vector<VkDescriptorImageInfo>;
        void loadScene(const std::string& path);
        void renderTick();
    };
}
