
# Sisi/GNSSLib/examples/CMakeLists.txt
# GNSSLib examples + USRP B210 streaming example

# ---------------------------------------------------------------------------
# Standard examples
# ---------------------------------------------------------------------------

foreach(EX basic_usage stream_usage param_usage)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EX}.cpp")
        add_executable(${EX} "${EX}.cpp")
        target_link_libraries(${EX} PRIVATE GNSSLib::lib Threads::Threads)
    endif()
endforeach()

# Copy config files next to basic_usage, if available.
if(TARGET basic_usage AND EXISTS "${GNSSLIB_ROOT}/configs")
    add_custom_command(TARGET basic_usage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${GNSSLIB_ROOT}/configs"
            "$<TARGET_FILE_DIR:basic_usage>/configs"
        COMMENT "Copying config files next to basic_usage"
    )
endif()

# ---------------------------------------------------------------------------
# USRP B210 streaming example
# ---------------------------------------------------------------------------

set(B210_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/gnsslib_usrp_b210_stream.cpp"
)

if(EXISTS "${B210_SOURCE}")
    find_package(UHD REQUIRED)

    add_executable(gnsslib_usrp_b210_stream "${B210_SOURCE}")

    target_link_libraries(gnsslib_usrp_b210_stream
        PRIVATE GNSSLib::lib Threads::Threads
    )

    if(TARGET UHD::uhd)
        target_link_libraries(gnsslib_usrp_b210_stream PRIVATE UHD::uhd)
    elseif(TARGET uhd)
        target_link_libraries(gnsslib_usrp_b210_stream PRIVATE uhd)
    else()
        target_include_directories(gnsslib_usrp_b210_stream
            PRIVATE ${UHD_INCLUDE_DIRS}
        )
        target_link_libraries(gnsslib_usrp_b210_stream
            PRIVATE ${UHD_LIBRARIES}
        )
    endif()

    message(STATUS "USRP B210 example enabled")
else()
    message(WARNING "B210 source not found: ${B210_SOURCE}")
endif()
