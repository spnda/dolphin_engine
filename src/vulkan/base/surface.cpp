#include "surface.hpp"

namespace dp {

Surface::Surface(Context& context) {
    surface = context.window->createSurface(context.instance.instance);
}

} // namespace dp
