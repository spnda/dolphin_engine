#pragma once

#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace dp {

/**
 * Simple abstraction over a SDL2 window, including helper
 * functions for various SDL functions.
 */
class Window {
private:
    static const SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;

    bool shouldQuit = false;

    int width;
    int height;

public:
    SDL_Window* window;
    SDL_DisplayMode mode;

    Window(std::string title, int width, int height);

    std::vector<const char*> getExtensions();

    VkSurfaceKHR createSurface(VkInstance vkInstance);

    bool shouldClose() {
        return shouldQuit;
    }

    void handleEvents();
};

} // namespace dp
