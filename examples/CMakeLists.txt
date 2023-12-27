if(BUILD_EXAMPLES)
    foreach(example "basic_output" "nft_output"
        )
    qt_add_executable( ${example}  ${example}.cpp)
    message(STATUS "Configuring example ${example}" )

    target_link_libraries(${example} PRIVATE wallet )

    set_target_properties(${example} PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
    )
endforeach()

endif(BUILD_EXAMPLES)
