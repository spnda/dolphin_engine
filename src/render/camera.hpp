#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../vulkan/context.hpp"
#include "../vulkan/resource/buffer.hpp"

namespace dp {
    class Camera {
    private:
        const glm::vec3 vecX = glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 vecY = glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 vecZ = glm::vec3(0.0f, 0.0f, 1.0f);

        const dp::Context ctx;
        dp::Buffer cameraBuffer;

        /** The data that will be in the buffer we use in the shaders */
        struct CameraBufferData {
            glm::mat4 viewInverse;
            glm::mat4 projectionInverse;
        } cameraBufferData;

        bool flipY = false;
        float fov = 70.0f;
        float zNear = 0.01f, zFar = 512.0f;
        glm::vec3 rotation = glm::vec3();
        glm::vec3 position = glm::vec3();

        /** Recalculates all matrices */
        void updateMatrices();

    public:
        static const uint32_t bufferSize = sizeof(CameraBufferData);

        Camera(const dp::Context ctx);

        /** Updates the descriptor buffer with new matrices */
        void updateBuffer();

        VkDescriptorBufferInfo getDescriptorInfo() const;

        Camera& setPerspective(const float fov, const float near, const float far);

        Camera& setAspectRatio(const float ratio);

        Camera& setPosition(const glm::vec3 pos);

        Camera& setRotation(const glm::vec3 rot);

        Camera& rotate(const glm::vec3 delta);
    };

} // namespace dp
