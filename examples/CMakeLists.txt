
find_package(Qt6 REQUIRED COMPONENTS Core Network)
foreach(example "basic_output"
		)
	qt_add_executable( ${example}  ${example}.cpp)
	message(STATUS "Configuring example ${example}" )
	# Link the used libraies in the executable
        target_link_libraries(${example} PRIVATE qwallet )

        set_target_properties(${example} PROPERTIES
                WIN32_EXECUTABLE ON
                MACOSX_BUNDLE ON
                )
        if (WIN32)
                add_custom_command(
                        TARGET ${example} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${example}> $<TARGET_FILE_DIR:${example}>
                        COMMAND_EXPAND_LISTS
                        )
        endif (WIN32)

	if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
		message(STATUS "Adding Test: ${example}" )
		add_test(NAME ${example} COMMAND ${example} https://https://api.testnet.shimmer.network ef4593558d0c3ed9e3f7a2de766d33093cd72372c800fa47ab5765c43ca006b5)
	endif(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
endforeach()

