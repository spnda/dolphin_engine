#include "engine.hpp"

auto main(int argc, char* argv[]) -> int {
    dp::Context ctx = dp::ContextBuilder::create("Dolphin")
        .build();
    
    dp::Engine engine(ctx);
    engine.renderLoop();
    return 0;
}
