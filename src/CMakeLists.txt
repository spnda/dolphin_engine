add_files(
    "main.cpp"
    "engine.cpp"
    "engine.hpp"
    "models/fileloader.cpp"
    "models/fileloader.hpp"
    "models/mesh.hpp"
    "models/modelmanager.cpp"
    "models/modelmanager.hpp"
    "render/camera.cpp"
    "render/camera.hpp"
    "render/ui.cpp"
    "render/ui.hpp"
    "sdl/window.cpp"
    "sdl/window.hpp"
    "vulkan/base/device.cpp"
    "vulkan/base/device.hpp"
    "vulkan/base/fence.hpp"
    "vulkan/base/fence.cpp"
    "vulkan/base/instance.cpp"
    "vulkan/base/instance.hpp"
    "vulkan/base/physical_device.cpp"
    "vulkan/base/physical_device.hpp"
    "vulkan/base/queue.cpp"
    "vulkan/base/queue.hpp"
    "vulkan/base/renderpass.cpp"
    "vulkan/base/renderpass.hpp"
    "vulkan/base/semaphore.cpp"
    "vulkan/base/semaphore.hpp"
    "vulkan/base/swapchain.cpp"
    "vulkan/base/swapchain.hpp"
    "vulkan/resource/buffer.cpp"
    "vulkan/resource/buffer.hpp"
    "vulkan/resource/image.cpp"
    "vulkan/resource/image.hpp"
    "vulkan/resource/stagingbuffer.cpp"
    "vulkan/resource/stagingbuffer.hpp"
    "vulkan/resource/storageimage.cpp"
    "vulkan/resource/storageimage.hpp"
    "vulkan/resource/texture.cpp"
    "vulkan/resource/texture.hpp"
    "vulkan/rt/acceleration_structure.cpp"
    "vulkan/rt/acceleration_structure.hpp"
    "vulkan/rt/rt_pipeline.cpp"
    "vulkan/rt/rt_pipeline.hpp"
    "vulkan/shaders/file_includer.cpp"
    "vulkan/shaders/file_includer.hpp"
    "vulkan/shaders/shader.cpp"
    "vulkan/shaders/shader.hpp"
    "vulkan/shaders/shader_database.cpp"
    "vulkan/shaders/shader_database.hpp"
    "vulkan/context.cpp"
    "vulkan/context.hpp"
    "vulkan/utils.hpp"
)

if (WITH_NV_AFTERMATH)
    add_files(
        "vulkan/base/crash_tracker.cpp"
        "vulkan/base/crash_tracker.hpp"
    )
endif()
