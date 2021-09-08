#pragma once

#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace dp {

// fwd
class Camera;

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

    void handleEvents(dp::Camera& camera);
};

} // namespace dp
