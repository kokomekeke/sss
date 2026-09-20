# ============================================================================
# Sisi/GNSSLib/CMakeLists.txt
#
# Cross-platform build (Linux / macOS / Windows MSVC) for:
#   - libGNSSLib (shared + static)   incl. streaming IfGenEngine + C API
#   - IFdataGen (standalone CLI, built on the library)
#   - RinexActualizerApp (standalone utility)
#   - examples
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(GNSSLib
    VERSION 1.0.0
    DESCRIPTION "Unified GNSS Signal Processing Library"
    LANGUAGES C CXX
)

include(GNUInstallDirs)

# ============================================================================
# C++ standard / build type
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_property(GNSSLIB_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT GNSSLIB_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# Options
# ============================================================================

option(GNSSLIB_BUILD_SHARED   "Build shared library"          ON)
option(GNSSLIB_BUILD_STATIC   "Build static library"          ON)
option(GNSSLIB_BUILD_EXAMPLES "Build examples"                ON)
option(GNSSLIB_BUILD_TOOLS    "Build standalone utilities"    ON)
option(GNSSLIB_INSTALL        "Generate install targets"      ON)
option(USE_NATIVE_OPT         "Tune code for build-host CPU"  ON)

# ============================================================================
# Paths
# ============================================================================

set(GNSSLIB_ROOT         "${CMAKE_CURRENT_SOURCE_DIR}")
set(SIGNALSIM_ROOT       "${CMAKE_CURRENT_SOURCE_DIR}/../SignalSim")
set(SIGNALSIM_SRC_DIR    "${SIGNALSIM_ROOT}/src")
set(SIGNALSIM_INC_DIR    "${SIGNALSIM_ROOT}/inc")
set(RINEX_ACTUALIZER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RinexActualizer")

# Output directories.
# The per-config subdirectories are only used by multi-config generators
# (Visual Studio). With Ninja/Make they would override the plain
# directories and put the binaries into bin/Release instead of bin/.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(GNSSLIB_MULTI_CONFIG)
    foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG}")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG}")
    endforeach()
endif()

# ============================================================================
# OpenMP / Threads
# ============================================================================

find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP: ENABLED (${OpenMP_CXX_VERSION})")
else()
    message(WARNING "OpenMP not found - building without OpenMP (single-threaded)")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ============================================================================
# Sources
# ============================================================================

file(GLOB SIGNALSIM_SOURCES CONFIGURE_DEPENDS "${SIGNALSIM_SRC_DIR}/*.cpp")
list(FILTER SIGNALSIM_SOURCES EXCLUDE REGEX ".*[Mm]ain\\.cpp$")
message(STATUS "SignalSim sources found: ${SIGNALSIM_SOURCES}")

# ============================================================================
# RINEX Actualizer (static, only for the standalone RinexActualizerApp;
# the same source is compiled directly into GNSSLib below, so no target
# ever links both -> no duplicate symbols)
# ============================================================================

add_library(RinexActualizer STATIC "${RINEX_ACTUALIZER_DIR}/ActualizeRinex.cpp")
target_include_directories(RinexActualizer PUBLIC "${RINEX_ACTUALIZER_DIR}" "${SIGNALSIM_INC_DIR}")
target_compile_features(RinexActualizer PUBLIC cxx_std_20)

# ============================================================================
# GNSSLib: compile once (OBJECT), link into shared + static
# ============================================================================

add_library(GNSSLib_objects OBJECT
    "${GNSSLIB_ROOT}/src/GNSSGenerator.cpp"
    "${GNSSLIB_ROOT}/src/IfGenEngine.cpp"
    ${SIGNALSIM_SOURCES}
    "${RINEX_ACTUALIZER_DIR}/ActualizeRinex.cpp"
)
set_target_properties(GNSSLib_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(GNSSLib_objects PUBLIC cxx_std_20)
target_include_directories(GNSSLib_objects PRIVATE
    "${GNSSLIB_ROOT}/include"
    "${SIGNALSIM_INC_DIR}"
    "${RINEX_ACTUALIZER_DIR}"
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(GNSSLib_objects PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(GNSSLib_objects PRIVATE USE_OPENMP)
endif()

# Helper: usage requirements that the final library targets need
function(gnsslib_finalize_target TGT)
    target_include_directories(${TGT} PUBLIC
        $<BUILD_INTERFACE:${GNSSLIB_ROOT}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${TGT} PRIVATE Threads::Threads)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${TGT} PUBLIC OpenMP::OpenMP_CXX)
    endif()
endfunction()

if(GNSSLIB_BUILD_SHARED)
    add_library(GNSSLib SHARED $<TARGET_OBJECTS:GNSSLib_objects>)
    set_target_properties(GNSSLib PROPERTIES
        OUTPUT_NAME "GNSSLib"
        VERSION     ${PROJECT_VERSION}
        SOVERSION   ${PROJECT_VERSION_MAJOR}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    gnsslib_finalize_target(GNSSLib)
endif()

if(GNSSLIB_BUILD_STATIC)
    add_library(GNSSLib_static STATIC $<TARGET_OBJECTS:GNSSLib_objects>)
    # On Windows the DLL import library is also GNSSLib.lib -> keep names apart
    if(WIN32)
        set_target_properties(GNSSLib_static PROPERTIES OUTPUT_NAME "GNSSLib_static")
    else()
        set_target_properties(GNSSLib_static PROPERTIES OUTPUT_NAME "GNSSLib")
    endif()
    gnsslib_finalize_target(GNSSLib_static)
endif()

# Alias used by the tools/examples: prefer static (self-contained executable)
if(TARGET GNSSLib_static)
    add_library(GNSSLib::lib ALIAS GNSSLib_static)
elseif(TARGET GNSSLib)
    add_library(GNSSLib::lib ALIAS GNSSLib)
endif()

# ============================================================================
# Standalone utilities
# ============================================================================

if(GNSSLIB_BUILD_TOOLS)

    # --- IFdataGen: thin CLI on top of the streaming library ------------------

    if(EXISTS "${GNSSLIB_ROOT}/tools/IFdataGen.cpp" AND TARGET GNSSLib::lib)
        add_executable(IFdataGen "${GNSSLIB_ROOT}/tools/IFdataGen.cpp")
        target_link_libraries(IFdataGen PRIVATE GNSSLib::lib)

        if(UNIX AND NOT APPLE)
            set_target_properties(IFdataGen PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
        elseif(APPLE)
            set_target_properties(IFdataGen PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
        endif()
        message(STATUS "IFdataGen: ENABLED")
    endif()

    # --- RinexActualizerApp ---------------------------------------------------

    if(EXISTS "${RINEX_ACTUALIZER_DIR}/RinexActualizerMain.cpp")
        add_executable(RinexActualizerApp "${RINEX_ACTUALIZER_DIR}/RinexActualizerMain.cpp")
        target_include_directories(RinexActualizerApp PRIVATE "${RINEX_ACTUALIZER_DIR}" "${SIGNALSIM_INC_DIR}")
        target_link_libraries(RinexActualizerApp PRIVATE RinexActualizer)
        if(OpenMP_CXX_FOUND)
            target_link_libraries(RinexActualizerApp PUBLIC OpenMP::OpenMP_CXX)
        endif()
        message(STATUS "RinexActualizerApp: ENABLED")
    else()
        message(STATUS "RinexActualizerMain.cpp not found - skipping RinexActualizerApp")
    endif()

endif()

# ============================================================================
# Examples
# ============================================================================

if(GNSSLIB_BUILD_EXAMPLES)
    if(EXISTS "${GNSSLIB_ROOT}/examples/CMakeLists.txt")
        add_subdirectory(examples)
    else()
        message(STATUS "examples/CMakeLists.txt not found - skipping examples")
    endif()
endif()

# ============================================================================
# Compiler flags
#
# NOTE: multi-flag generator expressions must use ';' (not spaces) as
# separator, otherwise CMake splits them at the space and leaks bare
# flags (e.g. "/permissive-") into every compiler's command line.
# ============================================================================

set(GNSSLIB_WARNINGS
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/wd4251>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
)

set(GNSSLIB_COMMON_DEFS "")
set(GNSSLIB_COMMON_OPTS "")
if(MSVC)
    set(GNSSLIB_COMMON_DEFS
        _USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
    )
    set(GNSSLIB_COMMON_OPTS /EHsc /utf-8 /bigobj)
endif()

# Targets that compile sources
set(COMPILE_TARGETS GNSSLib_objects RinexActualizer)
foreach(T IFdataGen RinexActualizerApp)
    if(TARGET ${T})
        list(APPEND COMPILE_TARGETS ${T})
    endif()
endforeach()

foreach(TGT ${COMPILE_TARGETS})
    target_compile_options(${TGT} PRIVATE ${GNSSLIB_WARNINGS} ${GNSSLIB_COMMON_OPTS})
    target_compile_definitions(${TGT} PRIVATE ${GNSSLIB_COMMON_DEFS})

    if(MSVC)
        target_compile_options(${TGT} PRIVATE
            $<$<CONFIG:Release>:/O2;/fp:fast;/Ob3;/Zi>
            $<$<CONFIG:RelWithDebInfo>:/O2;/Zi>
            $<$<CONFIG:Debug>:/Od;/Zi>
        )
        if(USE_NATIVE_OPT)
            target_compile_options(${TGT} PRIVATE /arch:AVX2)
        endif()
    else()
        target_compile_options(${TGT} PRIVATE
            $<$<CONFIG:Release>:-O3;-ffast-math;-funroll-loops;-fstrict-aliasing>
            $<$<CONFIG:RelWithDebInfo>:-O3;-g>
            $<$<CONFIG:Debug>:-O0;-g>
        )
        if(USE_NATIVE_OPT)
            target_compile_options(${TGT} PRIVATE -march=native -mtune=native)
        endif()
    endif()
endforeach()

# Debug info for MSVC link (final binaries)
if(MSVC)
    foreach(TGT GNSSLib IFdataGen RinexActualizerApp)
        if(TARGET ${TGT})
            target_link_options(${TGT} PRIVATE
                $<$<CONFIG:Release>:/DEBUG>
                $<$<CONFIG:RelWithDebInfo>:/DEBUG>
            )
        endif()
    endforeach()
endif()

# ============================================================================
# IPO / LTO
# ============================================================================

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_OK OUTPUT IPO_ERR)

if(IPO_OK)
    set(IPO_TARGETS ${COMPILE_TARGETS})
    foreach(T GNSSLib GNSSLib_static)
        if(TARGET ${T})
            list(APPEND IPO_TARGETS ${T})
        endif()
    endforeach()
    foreach(TGT ${IPO_TARGETS})
        set_property(TARGET ${TGT} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET ${TGT} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endforeach()
    message(STATUS "IPO/LTO: ENABLED")
else()
    message(STATUS "IPO/LTO: NOT SUPPORTED (${IPO_ERR})")
endif()

# ============================================================================
# Install
# ============================================================================

if(GNSSLIB_INSTALL)

    if(TARGET GNSSLib)
        install(TARGETS GNSSLib
            EXPORT      GNSSLibTargets
            LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    if(TARGET GNSSLib_static)
        install(TARGETS GNSSLib_static
            EXPORT      GNSSLibTargets
            ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    install(TARGETS RinexActualizer ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    if(TARGET IFdataGen)
        install(TARGETS IFdataGen RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    if(TARGET RinexActualizerApp)
        install(TARGETS RinexActualizerApp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    # Headers: gnss/gnsslib.h + gnss/IfGenEngine.h (no SignalSim types leak out)
    install(DIRECTORY "${GNSSLIB_ROOT}/include/gnss"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
    install(FILES "${RINEX_ACTUALIZER_DIR}/ActualizeRinex.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gnss
    )

    # CMake package config
    install(EXPORT GNSSLibTargets
        FILE        GNSSLibTargets.cmake
        NAMESPACE   GNSSLib::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GNSSLib
    )

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${GNSSLIB_ROOT}/cmake/GNSSLibConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/GNSSLibConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GNSSLib
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/GNSSLibConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/GNSSLibConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/GNSSLibConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GNSSLib
    )

    # pkg-config
    configure_file(
        "${GNSSLIB_ROOT}/cmake/gnsslib.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/gnsslib.pc"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/gnsslib.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()

# ============================================================================
# Status
# ============================================================================

message(STATUS "")
message(STATUS "==============================================")
message(STATUS " GNSSLib Configuration Summary")
message(STATUS "==============================================")
message(STATUS " Platform          : ${CMAKE_SYSTEM_NAME}")
message(STATUS " Compiler          : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS " C++ standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS " OpenMP            : ${OpenMP_CXX_FOUND}")
message(STATUS " IPO/LTO           : ${IPO_OK}")
message(STATUS " Native opt        : ${USE_NATIVE_OPT}")
message(STATUS " Shared library    : ${GNSSLIB_BUILD_SHARED}")
message(STATUS " Static library    : ${GNSSLIB_BUILD_STATIC}")
message(STATUS " Examples          : ${GNSSLIB_BUILD_EXAMPLES}")
message(STATUS " Standalone tools  : ${GNSSLIB_BUILD_TOOLS}")
message(STATUS "----------------------------------------------")
message(STATUS " SignalSim dir     : ${SIGNALSIM_ROOT}")
message(STATUS " RINEX Actualizer  : ${RINEX_ACTUALIZER_DIR}")
message(STATUS "==============================================")
message(STATUS "")