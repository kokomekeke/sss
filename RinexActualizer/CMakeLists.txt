cmake_minimum_required(VERSION 3.16)

project(RinexActualizerStandalone LANGUAGES C CXX)

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

# ============================================================================
# Paths
#
# Ez a fájl a Sisi/RinexActualizer/ mappában van -- az útvonalak innen
# számítódnak, NEM az IFdataGen mappából.
# ============================================================================

set(RINEX_ACTUALIZER_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SIGNALSIM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SignalSim/src")
set(SIGNALSIM_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SignalSim/inc")

# ============================================================================
# OpenMP
# ============================================================================

find_package(OpenMP QUIET)

# ============================================================================
# SignalSim sources
#
# A RinexActualizerMain.cpp közvetlenül hívja a CNavData, JsonInterpreter,
# GnssTime stb. függvényeit -- ezeket a forrásokat is be kell linkelni,
# nem elég a RinexActualizer library.
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
# Standalone RinexActualizer executable
# ============================================================================

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

target_link_libraries(
    RinexActualizerApp
    PRIVATE
        RinexActualizer
)

# ============================================================================
# OpenMP
# ============================================================================

if(OpenMP_CXX_FOUND)
    target_link_libraries(RinexActualizer PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(RinexActualizerApp PUBLIC OpenMP::OpenMP_CXX)
endif()

# ============================================================================
# IPO / LTO
# ============================================================================

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_OK OUTPUT IPO_ERR)

if(IPO_OK)
    set_property(TARGET RinexActualizer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET RinexActualizerApp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ============================================================================
# MSVC / GCC / Clang optimization
# ============================================================================

if(MSVC)

    foreach(TGT RinexActualizer RinexActualizerApp)
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

    foreach(TGT RinexActualizer RinexActualizerApp)
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
# Status
# ============================================================================

message(STATUS "")
message(STATUS "==============================================")
message(STATUS " RinexActualizer (standalone build)")
message(STATUS "==============================================")
message(STATUS "SignalSim src dir    : ${SIGNALSIM_SRC_DIR}")
message(STATUS "SignalSim inc dir    : ${SIGNALSIM_INC_DIR}")
message(STATUS "OpenMP found         : ${OpenMP_CXX_FOUND}")
message(STATUS "IPO / LTO supported  : ${IPO_OK}")
message(STATUS "Native optimization  : ${USE_NATIVE_OPT}")
message(STATUS "==============================================")
message(STATUS "")