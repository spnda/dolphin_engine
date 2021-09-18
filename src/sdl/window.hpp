#pragma once

#include <string>
#include <vector>
#include <functional>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace dp {

// fwd
class Camera;

// fwd
class Engine;

/**
 * Simple abstraction over a SDL2 window, including helper
 * functions for various SDL functions.
 */
class Window {
private:
    static const SDL_WindowFlags windowFlags =
        SDL_WindowFlags(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    bool shouldQuit = false;

    int width;
    int height;

    struct Keys {
        bool forward_pressed;
        bool backward_pressed;
    } keys_pressed;

public:
    SDL_Window* window;
    SDL_DisplayMode mode;

    Window(std::string title, int width, int height);

    ~Window();

    std::vector<const char*> getExtensions();

    VkSurfaceKHR createSurface(VkInstance vkInstance);

    bool shouldClose();

    float getAspectRatio();

    void handleEvents(dp::Engine& engine);
};

} // namespace dp
