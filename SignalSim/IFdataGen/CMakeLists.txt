cmake_minimum_required(VERSION 3.16)

project(SisiGnssTools LANGUAGES C CXX)

# ============================================================================
# C++ standard
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Options
# ============================================================================

option(USE_NATIVE_OPT "Tune code for the build-host CPU" ON)
option(BUILD_RINEX_ACTUALIZER "Also build standalone RinexActualizerApp here" OFF)

# ============================================================================
# Paths
#
# Ez a fájl a Sisi/SignalSim/IFdataGen/ mappában van.
# ============================================================================

set(SIGNALSIM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(SIGNALSIM_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../inc")
set(RINEX_ACTUALIZER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../RinexActualizer")

# ============================================================================
# OpenMP
# ============================================================================

find_package(OpenMP QUIET)

# ============================================================================
# SignalSim sources
# ============================================================================

file(GLOB SIGNALSIM_SOURCES CONFIGURE_DEPENDS
    "${SIGNALSIM_SRC_DIR}/*.cpp"
)

# ============================================================================
# RINEX Actualizer library
# ============================================================================

add_library(
    RinexActualizer
    STATIC
    "${RINEX_ACTUALIZER_DIR}/ActualizeRinex.cpp"
)

target_include_directories(
    RinexActualizer
    PUBLIC
        "${RINEX_ACTUALIZER_DIR}"
        "${SIGNALSIM_INC_DIR}"
)

target_compile_features(RinexActualizer PUBLIC cxx_std_20)

# ============================================================================
# IFdataGen executable
# ============================================================================

add_executable(
    IFdataGen
    "${CMAKE_CURRENT_SOURCE_DIR}/IFdataGen.cpp"
    ${SIGNALSIM_SOURCES}
)

target_include_directories(
    IFdataGen
    PRIVATE
        "${SIGNALSIM_INC_DIR}"
        "${RINEX_ACTUALIZER_DIR}"
)

target_link_libraries(IFdataGen PRIVATE RinexActualizer)

# ============================================================================
# OpenMP
# ============================================================================

if(OpenMP_CXX_FOUND)
    target_link_libraries(IFdataGen PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(RinexActualizer PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(IFdataGen PRIVATE USE_OPENMP)
endif()

# ============================================================================
# IPO / LTO
# ============================================================================

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_OK OUTPUT IPO_ERR)

if(IPO_OK)
    set_property(TARGET IFdataGen PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET RinexActualizer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ============================================================================
# MSVC / GCC / Clang optimization
# ============================================================================

if(MSVC)

    foreach(TGT IFdataGen RinexActualizer)
        target_compile_options(${TGT} PRIVATE
            $<$<CONFIG:Release>:/O2 /Ot /fp:fast /Ob3 /Zi /permissive->
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
            $<$<CONFIG:Debug>:/Od /Zi>
        )
        if(USE_NATIVE_OPT)
            target_compile_options(${TGT} PRIVATE /arch:AVX2)
        endif()
    endforeach()

else()

    foreach(TGT IFdataGen RinexActualizer)
        target_compile_options(${TGT} PRIVATE
            $<$<CONFIG:Release>:-O3 -ffast-math -funroll-loops -fstrict-aliasing>
            $<$<CONFIG:RelWithDebInfo>:-O3 -g>
            $<$<CONFIG:Debug>:-O0 -g>
        )
        if(USE_NATIVE_OPT)
            target_compile_options(${TGT} PRIVATE -march=native -mtune=native)
        endif()
    endforeach()

endif()

# ============================================================================
# Optional standalone RinexActualizer executable (off by default here --
# use Sisi/RinexActualizer/CMakeLists.txt for the dedicated standalone build)
# ============================================================================

if(BUILD_RINEX_ACTUALIZER)

    add_executable(
        RinexActualizerApp
        "${RINEX_ACTUALIZER_DIR}/RinexActualizerMain.cpp"
        ${SIGNALSIM_SOURCES}
    )

    target_include_directories(
        RinexActualizerApp
        PRIVATE
            "${RINEX_ACTUALIZER_DIR}"
            "${SIGNALSIM_INC_DIR}"
    )

    target_link_libraries(RinexActualizerApp PRIVATE RinexActualizer)

    if(OpenMP_CXX_FOUND)
        target_link_libraries(RinexActualizerApp PUBLIC OpenMP::OpenMP_CXX)
    endif()

endif()

# ============================================================================
# Status
# ============================================================================

message(STATUS "")
message(STATUS "==============================================")
message(STATUS " Sisi GNSS Tools (IFdataGen integrated build)")
message(STATUS "==============================================")
message(STATUS "RINEX Actualizer dir : ${RINEX_ACTUALIZER_DIR}")
message(STATUS "OpenMP found         : ${OpenMP_CXX_FOUND}")
message(STATUS "IPO / LTO supported  : ${IPO_OK}")
message(STATUS "Native optimization  : ${USE_NATIVE_OPT}")
message(STATUS "Standalone actualizer: ${BUILD_RINEX_ACTUALIZER}")
message(STATUS "==============================================")
message(STATUS "")