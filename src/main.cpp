#include "engine.hpp"

auto main(int argc, char* argv[]) -> int {
    dp::Context ctx("Dolphin");
    ctx.init();
    dp::Engine engine(ctx);
    engine.renderLoop();
    return 0;
}
