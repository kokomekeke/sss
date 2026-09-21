# Sisi/GNSSLib/examples/CMakeLists.txt
# GNSSLib::lib (alias) = static lib if built, otherwise the shared one.
# The public headers no longer need the SignalSim include directory.

foreach(EX basic_usage stream_usage stream_test)
add_executable(${EX} ${EX}.cpp)
target_link_libraries(${EX} PRIVATE GNSSLib::lib)
endforeach()

# Copy config files next to the binaries, if they exist.
if(EXISTS "${GNSSLIB_ROOT}/configs")
add_custom_command(TARGET basic_usage POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${GNSSLIB_ROOT}/configs"
"$<TARGET_FILE_DIR:basic_usage>/configs"
COMMENT "Copying config files next to basic_usage"
)
else()
message(STATUS "examples: configs/ directory not found, skipping copy")
endif()
