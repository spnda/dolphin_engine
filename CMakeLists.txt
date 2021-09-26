cmake_minimum_required(VERSION 3.20)

# Automatically find our vcpkg.cmake toolchain file.
# This has to be defined before project()
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(vcpkg_file "$ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake")
  cmake_path(SET vcpkg_file NORMALIZE "${vcpkg_file}")
  set(CMAKE_TOOLCHAIN_FILE "${vcpkg_file}" CACHE FILEPATH "")
endif()

# Define the path to the glsl validator.
if (NOT DEFINED GLSL_COMPILER)
  # It's 2021, nobody is using a 32-bit machine anymore.
  set(GLSL_COMPILER "$ENV{VULKAN_SDK}/Bin/glslc.exe" CACHE FILEPATH "")
endif()

project("dolphin_engine")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<0:>") # The $<0:> is a generator expression to remove the Debug/Release folders.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

# Set our module path to be the 'cmake' dir and load all files.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
include(add_files)
include(compile_shaders)

# Find dependencies
find_package(Vulkan REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(assimp CONFIG REQUIRED)

add_executable(dolphin_engine)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_SOURCE_DIR}/vk-bootstrap)

target_link_libraries(dolphin_engine PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(dolphin_engine PRIVATE Vulkan::Vulkan PNG::PNG)
target_link_libraries(dolphin_engine PRIVATE unofficial::vulkan-memory-allocator::vulkan-memory-allocator)
target_link_libraries(dolphin_engine PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(dolphin_engine PRIVATE imgui::imgui)
target_link_libraries(dolphin_engine PRIVATE glm::glm)
target_link_libraries(dolphin_engine PRIVATE assimp::assimp)

if (MSVC)
  # Warn about nested declarations
  add_compile_options("/w34456")
  # Warn about potentially uninitialized variables
  add_compile_options("/w34701")
  add_compile_options("/w34703")
  # Warn about different indirection types.
  add_compile_options("/w34057")
  # Warn about signed/unsigned mismatch.
  add_compile_options("/w34245")
endif()

# Compile shaders
set(SHADER_FILES closesthit.rchit miss.rmiss raygen.rgen)
foreach(SHADER ${SHADER_FILES})
  set(SHADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}")
  message(STATUS "Configuring shader ${SHADER_FILE}")
  cmake_path(GET SHADER_FILE FILENAME SHADER_FILE_NAME)
  set(SHADER_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${SHADER_FILE_NAME}.spv")
  add_custom_command(
    # TARGET dolphin_engine
    # POST_BUILD
    OUTPUT ${SHADER_OUTPUT}
    COMMAND ${GLSL_COMPILER} -o ${SHADER_OUTPUT} --target-spv=spv1.5 ${SHADER_FILE}
    DEPENDS ${SHADER_FILE}
    VERBATIM
  )
  list(APPEND SPIRV_BINARY_FILES ${SHADER_OUTPUT})
endforeach(SHADER)

add_custom_target(
  shaders ALL
  DEPENDS ${SPIRV_BINARY_FILES}
)
