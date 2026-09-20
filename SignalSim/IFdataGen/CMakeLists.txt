cmake_minimum_required(VERSION 3.16)

# Sisi/SignalSim/IFdataGen/CMakeLists.txt

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

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP: ENABLED")
else()
    message(WARNING "OpenMP not found - building without OpenMP")
endif()

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
#
# Csak Release / RelWithDebInfo alatt kapcsoljuk be - Debug alatt csak
# lassítaná a buildet és rontaná a debugolhatóságot (ez MSVC alatt is igaz).
# ============================================================================

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_OK OUTPUT IPO_ERR)

if(IPO_OK)
    foreach(TGT IFdataGen RinexActualizer)
        set_property(TARGET ${TGT} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET ${TGT} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endforeach()
endif()

# ============================================================================
# Windows / MSVC specifikus beállítások
#
# - /EHsc            : szabványos C++ kivételkezelés bekapcsolása (enélkül
#                       az MSVC alapból nem garantálja a helyes kivétel-
#                       terjedést és warningokat/hibákat dobhat)
# - /utf-8            : a forrás UTF-8 (a kommentek/stringek között magyar
#                       ékezetes karakterek is vannak) - GCC/Clang alatt ez
#                       alapértelmezett, MSVC alatt explicit kérni kell
# - _USE_MATH_DEFINES : M_PI és társai csak ezzel érhetők el <cmath> alól
#                       MSVC alatt (GCC/Clang alatt nem szükséges)
# - NOMINMAX,
#   WIN32_LEAN_AND_MEAN : ha valamelyik header lánc <windows.h>-t húzna be,
#                       ezek nélkül a min()/max() makrók összeakadhatnak a
#                       std::min/std::max-al
# - _CRT_SECURE_NO_WARNINGS : a klasszikus C-függvények (sprintf, fopen stb.)
#                       MSVC "biztonsági" figyelmeztetéseinek elnémítása,
#                       hogy a build ne fulladjon warning-áradatba
# ============================================================================

if(MSVC)
    foreach(TGT IFdataGen RinexActualizer)
        target_compile_options(${TGT} PRIVATE /EHsc /utf-8)
        target_compile_definitions(${TGT} PRIVATE
            _USE_MATH_DEFINES
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
    endforeach()
endif()

# ============================================================================
# MSVC / GCC / Clang optimization
# ============================================================================

if(MSVC)

    foreach(TGT IFdataGen RinexActualizer)
        target_compile_options(${TGT} PRIVATE
            # Megjegyzés: /Ot elavult MSVC kapcsoló volt, /O2 már tartalmazza
            # a sebesség-fókuszú optimalizálást, ezért kivettük.
            $<$<CONFIG:Release>:/O2 /fp:fast /Ob3 /Zi /permissive->
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
            $<$<CONFIG:Debug>:/Od /Zi>
        )
        # Release/RelWithDebInfo alatt engedjük a linkelt PDB generálást,
        # hogy /Zi mellett a végeredmény is debug-olható maradjon.
        target_link_options(${TGT} PRIVATE
            $<$<CONFIG:Release>:/DEBUG>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG>
        )
        if(USE_NATIVE_OPT)
            # FIGYELEM: /arch:AVX2 csak akkor biztonságos, ha a célgép CPU-ja
            # (amin a binárist futtatod) is támogatja az AVX2-t. Ha nem vagy
            # benne biztos, vagy másik gépre is szánod a buildet, kapcsold ki:
            #   cmake .. -DUSE_NATIVE_OPT=OFF
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

    if(MSVC)
        target_compile_options(RinexActualizerApp PRIVATE /EHsc /utf-8)
        target_compile_definitions(RinexActualizerApp PRIVATE
            _USE_MATH_DEFINES
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
    endif()

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
message(STATUS "Platform             : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler             : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "RINEX Actualizer dir : ${RINEX_ACTUALIZER_DIR}")
message(STATUS "OpenMP found         : ${OpenMP_CXX_FOUND}")
message(STATUS "IPO / LTO supported  : ${IPO_OK}")
message(STATUS "Native optimization  : ${USE_NATIVE_OPT}")
message(STATUS "Standalone actualizer: ${BUILD_RINEX_ACTUALIZER}")
message(STATUS "==============================================")
message(STATUS "")