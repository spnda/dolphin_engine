#include <algorithm>

#include "window.hpp"

#include "../render/camera.hpp"

dp::Window::Window(std::string title, int width, int height) : width(width), height(height) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_SetRelativeMouseMode(SDL_TRUE); // We want just relative mouse coordinates for movement.

    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width, height,
        windowFlags);

    SDL_GetCurrentDisplayMode(0, &this->mode);
}

dp::Window::~Window() {
    SDL_Quit();
}

std::vector<const char*> dp::Window::getExtensions() {
        // We don't know the size to begin with, so we'll query the extensions twice,
        // once for the size and once for the actual data.
        uint32_t count = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
        std::vector<const char*> extensions(count);
        SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());
        return extensions;
}

VkSurfaceKHR dp::Window::createSurface(VkInstance vkInstance) {
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, vkInstance, &surface);
    return surface;
}

bool dp::Window::shouldClose() {
    return shouldQuit;
}

float dp::Window::getAspectRatio() {
    return (float)width / (float)height;
}

void dp::Window::handleEvents(dp::Camera& camera) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                shouldQuit = true;
                break;
            case SDL_MOUSEMOTION:
                if (!(event.motion.state & SDL_BUTTON_LMASK)) break;
                glm::vec3 motion = glm::vec3(-event.motion.yrel, event.motion.xrel, 0.0f);
                camera.rotate(motion);
                break;
            case SDL_MOUSEWHEEL:
                camera.setFov(std::clamp(camera.getFov() * (event.wheel.y > 0 ? 0.99f : 1.01f), 0.0f, 140.0f));
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP) {
                    camera.move([&](glm::vec3& position, const glm::vec3 camFront) {
                        position += camFront * 1.0f;
                    });
                } else if (event.key.keysym.sym == SDLK_s || event.key.keysym.sym == SDLK_DOWN) {
                    camera.move([&](glm::vec3& position, const glm::vec3 camFront) {
                        position -= camFront * 1.0f;
                    });
                } else if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_LEFT) {
                    camera.move([&](glm::vec3& position, const glm::vec3 camFront) {
                        position -= glm::normalize(glm::cross(camFront, camera.vecY)) * 1.0f;
                    });
                } else if (event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT) {
                    camera.move([&](glm::vec3& position, const glm::vec3 camFront) {
                        position += glm::normalize(glm::cross(camFront, camera.vecY)) * 1.0f;
                    });
                }
                break;
        }
    }
}
