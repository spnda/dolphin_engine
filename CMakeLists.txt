cmake_minimum_required(VERSION 3.21)

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
if (MSVC)
  set(CMAKE_CXX_FLAGS "/Zc:__cplusplus /EHsc") # Microsoft not properly setting __cplusplus by default.
endif()

# Set our module path to be the 'cmake' dir and load all files.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
include(add_files)
include(compile_shaders)

# Find dependencies
find_package(assimp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb.h")
find_package(Vulkan REQUIRED)

add_executable(dolphin_engine)

# Add shaderc from the Vulkan SDK
add_library(shaderc UNKNOWN IMPORTED)
if(WIN32)
  set_target_properties(shaderc PROPERTIES IMPORTED_LOCATION $ENV{VULKAN_SDK}/Lib/shaderc_shared.lib)
  set_property(TARGET shaderc PROPERTY INTERFACE_INCLUDE_DIRECTORIES $ENV{VULKAN_SDK}/Include)
  target_link_libraries(dolphin_engine PRIVATE shaderc)
else()
  target_link_libraries(dolphin_engine PRIVATE shaderc_combined glslang MachineIndependent OSDependent OGLCompiler GenericCodeGen SPIRV SPIRV-Tools-opt SPIRV-Tools)
endif()

# If the Aftermath SDK has been provided and we're running debug, add it to the build.
string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/nv-aftermath AND build_type STREQUAL "debug")
  if (WIN32)
    message(STATUS "Found NVIDIA Aftermath")
    add_library(nv-aftermath UNKNOWN IMPORTED)
    set_target_properties(nv-aftermath PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/nv-aftermath/lib/x64/GFSDK_Aftermath_Lib.x64.lib)
    set_property(TARGET nv-aftermath PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/nv-aftermath/include)
    target_link_libraries(dolphin_engine PRIVATE nv-aftermath)
    option(WITH_NV_AFTERMATH "Use NVIDIA Aftermath" ON)
    add_compile_definitions(WITH_NV_AFTERMATH)
  endif() # Add non-windows support?
endif()

target_link_libraries(dolphin_engine PRIVATE assimp::assimp)
target_link_libraries(dolphin_engine PRIVATE fmt::fmt-header-only)
target_link_libraries(dolphin_engine PRIVATE glm::glm)
target_link_libraries(dolphin_engine PRIVATE imgui::imgui)
target_link_libraries(dolphin_engine PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_include_directories(dolphin_engine PRIVATE ${STB_INCLUDE_DIRS})
target_link_libraries(dolphin_engine PRIVATE PNG::PNG)
target_link_libraries(dolphin_engine PRIVATE Vulkan::Vulkan)
target_link_libraries(dolphin_engine PRIVATE vk-bootstrap::vk-bootstrap)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/vk-bootstrap)
add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/vk-mem-alloc)

# Copy shaders to bin directory.
set(SHADER_FILES closesthit.rchit miss.rmiss raygen.rgen include/raycommon.glsl)
foreach(SHADER ${SHADER_FILES})
  set(SHADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}")
  message(STATUS "Configuring shader ${SHADER_FILE}")
  set(SHADER_DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${SHADER}")
  add_custom_command(
          TARGET dolphin_engine
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_FILE} ${SHADER_DESTINATION}
  )
endforeach(SHADER)
