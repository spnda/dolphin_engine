#include <algorithm>

#include "window.hpp"

#include "../engine.hpp"

dp::Window::Window(const std::string& title, uint32_t width, uint32_t height)
        : width(width), height(height), mode{} {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_ShowCursor(SDL_ENABLE);

    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width, height,
        windowFlags);

    SDL_GetCurrentDisplayMode(0, &this->mode);
}

dp::Window::~Window() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

std::vector<const char*> dp::Window::getExtensions() const {
        // We don't know the size to begin with, so we'll query the extensions twice,
        // once for the size and once for the actual data.
        uint32_t count = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
        std::vector<const char*> extensions(count);
        SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());
        return extensions;
}

VkSurfaceKHR dp::Window::createSurface(VkInstance vkInstance) const {
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, vkInstance, &surface);
    return surface;
}

bool dp::Window::shouldClose() const {
    return shouldQuit;
}

float dp::Window::getAspectRatio() const {
    return (float)width / (float)height;
}

void dp::Window::handleEvents(dp::Engine& engine) {
    SDL_Event event;
    while ((engine.needsResize ? SDL_WaitEvent(&event) : SDL_PollEvent(&event)) != 0) {
        switch (event.type) {
            case SDL_QUIT:
                shouldQuit = true;
                break;
            case SDL_WINDOWEVENT: {
                // All events regarding the window.
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        engine.resize(event.window.data1, event.window.data2);
                        break;
                }
                break;
            }
            case SDL_MOUSEMOTION: {
                if (!(event.motion.state & SDL_BUTTON_LMASK)) break;
                if (dp::Ui::isInputting()) break; // If the user is currently using the UI.
                glm::vec3 motion(-event.motion.yrel, event.motion.xrel, 0.0f);
                engine.camera.rotate(motion);
                break;
            }
            case SDL_MOUSEWHEEL: {
                if (dp::Ui::isInputting()) break;
                engine.camera.setFov(
                        std::clamp(engine.camera.getFov() * (event.wheel.y > 0 ? 0.99f : 1.01f), 0.0f, 140.0f));
                break;
            }
            case SDL_KEYDOWN: {
                engine.camera.move([&](glm::vec3 &position, const glm::vec3 camFront) {
                    if (event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_UP) {
                        position += camFront * 1.0f;
                    } else if (event.key.keysym.sym == SDLK_s || event.key.keysym.sym == SDLK_DOWN) {
                        position -= camFront * 1.0f;
                    } else if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_LEFT) {
                        position -= glm::normalize(glm::cross(camFront, engine.camera.vecY)) * 1.0f;
                    } else if (event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_RIGHT) {
                        position += glm::normalize(glm::cross(camFront, engine.camera.vecY)) * 1.0f;
                    }
                });
                break;
            }
        }
    }
}
