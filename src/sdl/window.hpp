#pragma once

#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace dp {
    // fwd
    class Engine;

    /**
     * Simple abstraction over a SDL2 window, including helper
     * functions for various SDL functions.
     */
    class Window {
        static const SDL_WindowFlags windowFlags =
            SDL_WindowFlags(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        bool shouldQuit = false;

        uint32_t width;
        uint32_t height;

    public:
        SDL_Window* window;
        SDL_DisplayMode mode;

        Window(const std::string& title, uint32_t width, uint32_t height);

        ~Window();

        [[nodiscard]] std::vector<const char*> getExtensions() const;

        VkSurfaceKHR createSurface(VkInstance vkInstance) const;

        [[nodiscard]] bool shouldClose() const;

        [[nodiscard]] float getAspectRatio() const;

        void handleEvents(dp::Engine& engine);
    };
} // namespace dp
