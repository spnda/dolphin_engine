#include "camera.hpp"

dp::Camera::Camera(const dp::Context context)
        : ctx(context), cameraBuffer(ctx, "cameraBuffer") {
    cameraBufferData.projectionInverse = glm::mat4(1.0f);
    cameraBufferData.viewInverse = glm::mat4(1.0f);

    cameraBuffer.create(
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

void dp::Camera::updateMatrices() {
    // We won't update the projection matrix as that doesn't change, unless
    // setPerspective or setAspectRatio is called, which do it themselves anyway.
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.x), vecX);
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), vecY);
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), vecZ);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

    cameraBufferData.viewInverse = glm::inverse(rotationMatrix * translationMatrix);
}

void dp::Camera::updateBuffer() {
    this->updateMatrices();
    cameraBuffer.memoryCopy(&cameraBufferData, bufferSize);
}

VkDescriptorBufferInfo dp::Camera::getDescriptorInfo() const {
    return cameraBuffer.getDescriptorInfo(bufferSize, 0);
}

float dp::Camera::getFov() const {
    return this->fov;
}

dp::Camera& dp::Camera::setPerspective(const float fov, const float near, const float far) {
    this->fov = fov; this->zNear = near; this->zFar = far;
    auto perspective = glm::perspective(glm::radians(this->fov), ctx.window->getAspectRatio(), zNear, zFar);
    cameraBufferData.projectionInverse = glm::inverse(perspective);
    return *this;
}

dp::Camera& dp::Camera::setAspectRatio(const float ratio) {
    cameraBufferData.projectionInverse = glm::perspective(glm::radians(fov), ratio, zNear, zFar);
    if (flipY) {
        cameraBufferData.projectionInverse[1][1] *= -1.0f;
    }
    return *this;
}

dp::Camera& dp::Camera::setFov(const float fov) {
    this->setPerspective(fov, zNear, zFar);
    return *this;
}

dp::Camera& dp::Camera::setPosition(const glm::vec3 pos) {
    this->position = pos;
    return *this;
}

dp::Camera& dp::Camera::move(std::function<void(glm::vec3&, const glm::vec3)> callback) {
    glm::vec3 camera;
    camera.x = -std::cos(glm::radians(rotation.x)) * std::sin(glm::radians(rotation.y));
    camera.y = std::sin(glm::radians(rotation.x));
    camera.z = std::cos(glm::radians(rotation.x)) * std::cos(glm::radians(rotation.y));
    callback(this->position, camera);
    return *this;
}

dp::Camera& dp::Camera::setRotation(const glm::vec3 rot) {
    this->rotation = rot;
    return *this;
}

dp::Camera& dp::Camera::rotate(const glm::vec3 delta) {
    this->rotation += delta;
    return *this;
}
