#include "window.hpp"

dp::Window::Window(std::string title, int width, int height) : width(width), height(height) {
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width, height,
        windowFlags);

    SDL_GetCurrentDisplayMode(0, &this->mode);
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

void dp::Window::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                shouldQuit = true;
                break;
        }
    }
}
