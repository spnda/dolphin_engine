#pragma once

#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../vulkan/context.hpp"
#include "../vulkan/resource/buffer.hpp"

namespace dp {
    class Camera {
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
        const glm::vec3 vecX = glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 vecY = glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 vecZ = glm::vec3(0.0f, 0.0f, 1.0f);

        static const uint32_t bufferSize = sizeof(CameraBufferData);

        explicit Camera(const dp::Context& ctx);

        void destroy();

        /** Updates the descriptor buffer with new matrices */
        void updateBuffer();

        VkDescriptorBufferInfo getDescriptorInfo() const;

        float getFov() const;

        Camera& setPerspective(float fov, float near, float far);

        Camera& setAspectRatio(float ratio);

        Camera& setFov(float fov);

        Camera& setPosition(glm::vec3 pos);

        Camera& move(const std::function<void(glm::vec3&, const glm::vec3)>& callback);

        Camera& setRotation(glm::vec3 rot);

        Camera& rotate(glm::vec3 delta);
    };

} // namespace dp
