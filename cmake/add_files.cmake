function(add_files)
	cmake_parse_arguments(PARAM "" "" "CONDITION" ${ARGN})
	set(PARAM_FILES "${PARAM_UNPARSED_ARGUMENTS}")

	foreach(FILE IN LISTS PARAM_FILES)
		target_sources(dolphin_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
	endforeach()
endfunction()
