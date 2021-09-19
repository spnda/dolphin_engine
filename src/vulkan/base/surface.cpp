#include "surface.hpp"

#include "../context.hpp"
#include "../../sdl/window.hpp"

dp::Surface::Surface(const dp::Context& context) {
    surface = context.window->createSurface(context.instance);
}
