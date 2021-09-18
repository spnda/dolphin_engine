#include "surface.hpp"

dp::Surface::Surface(const dp::Context& context) {
    surface = context.window->createSurface(context.instance);
}
