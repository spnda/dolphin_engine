# NOTE: Doesn't seem to work here :(
function(compile_shaders)
	cmake_parse_arguments(PARAM "" "" "CONDITION" ${ARGN})
	set(PARAM_FILES "${PARAM_UNPARSED_ARGUMENTS}")

	foreach(SHADER IN LISTS PARAM_FILES)
        set(SHADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}") # Fix the directory for the file.
        message(STATUS "Configuring shader ${SHADER_FILE}")
        cmake_path(GET SHADER_FILE FILENAME SHADER_FILE_NAME)
        set(SHADER_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${SHADER_FILE_NAME}.spv")
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${GLSL_COMPILER} -o ${SHADER_OUTPUT} --target-spv=spv1.5 ${SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            VERBATIM
        )
        list(APPEND SPIRV_BINARY_FILES ${SHADER_OUTPUT})
    endforeach(SHADER)
endfunction()
