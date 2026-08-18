# ============================================================================
# Sisi GNSS Tools - Makefile
#
# Builds:
#   1. IFdataGen
#   2. RinexActualizerApp
#
# Common RINEX actualization code is compiled into:
#   libRinexActualizer.a
#
# Directory layout:
#
# Sisi/
# ├── RinexActualizer/
# │   ├── ActualizeRinex.cpp
# │   ├── ActualizeRinex.h
# │
# │
# └── SignalSim/
#     ├── src/
#     ├── inc/
#     └── IFdataGen/
#
# ============================================================================


# ----------------------------------------------------------------------------
# Compiler
# ----------------------------------------------------------------------------

CXX ?= g++
CC  ?= gcc


# ----------------------------------------------------------------------------
# Targets
# ----------------------------------------------------------------------------

IFDATA_TARGET = IFdataGen
ACTUALIZER_TARGET = RinexActualizerApp

LIB_TARGET = libRinexActualizer.a


# ----------------------------------------------------------------------------
# Directories
# ----------------------------------------------------------------------------

SRCDIR = ../src
INCDIR = ../inc

ACTUALIZER_DIR = ../../RinexActualizer

OBJDIR = obj
BINDIR = .
LIBDIR = lib


# ----------------------------------------------------------------------------
# C++ standard
# ----------------------------------------------------------------------------

CXXFLAGS = -std=c++20


# ----------------------------------------------------------------------------
# Warning flags
# ----------------------------------------------------------------------------

CXXFLAGS += -Wall
CXXFLAGS += -Wextra
CXXFLAGS += -Wpedantic


# ----------------------------------------------------------------------------
# Optimization
# ----------------------------------------------------------------------------

CXXFLAGS += -O3
CXXFLAGS += -ffast-math
CXXFLAGS += -funroll-loops
CXXFLAGS += -fstrict-aliasing


# ----------------------------------------------------------------------------
# Include paths
# ----------------------------------------------------------------------------

CXXFLAGS += -I$(INCDIR)
CXXFLAGS += -I$(ACTUALIZER_DIR)


# ----------------------------------------------------------------------------
# OpenMP
# ----------------------------------------------------------------------------

OPENMP_TEST := $(shell \
	echo 'int main() { return 0; }' | \
	$(CXX) -fopenmp -x c++ - -o /dev/null 2>/dev/null && \
	echo yes || echo no)

ifeq ($(OPENMP_TEST),yes)

    CXXFLAGS += -fopenmp
    LDFLAGS += -fopenmp
    $(info OpenMP support: ENABLED)

else

    $(info OpenMP support: DISABLED)

endif


# ----------------------------------------------------------------------------
# Platform
# ----------------------------------------------------------------------------

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)

    CXXFLAGS += -pthread
    LDFLAGS += -pthread

endif


ifeq ($(UNAME_S),Darwin)

    # Uncomment if using Homebrew libomp:
    #
    # CXXFLAGS += -I/opt/homebrew/opt/libomp/include
    # LDFLAGS += -L/opt/homebrew/opt/libomp/lib
    # LDFLAGS += -lomp

endif


# ----------------------------------------------------------------------------
# SignalSim sources
# ----------------------------------------------------------------------------

SIGNALSIM_SOURCES = \
    $(SRCDIR)/SatIfSignal.cpp \
    $(SRCDIR)/Almanac.cpp \
    $(SRCDIR)/BCNav1Bit.cpp \
    $(SRCDIR)/BCNav2Bit.cpp \
    $(SRCDIR)/BCNav3Bit.cpp \
    $(SRCDIR)/BCNavBit.cpp \
    $(SRCDIR)/CNav2Bit.cpp \
    $(SRCDIR)/CNavBit.cpp \
    $(SRCDIR)/ComplexNumber.cpp \
    $(SRCDIR)/Coordinate.cpp \
    $(SRCDIR)/D1D2NavBit.cpp \
    $(SRCDIR)/FNavBit.cpp \
    $(SRCDIR)/GNavBit.cpp \
    $(SRCDIR)/GnssTime.cpp \
    $(SRCDIR)/INavBit.cpp \
    $(SRCDIR)/JsonInterpreter.cpp \
    $(SRCDIR)/JsonParser.cpp \
    $(SRCDIR)/LNavBit.cpp \
    $(SRCDIR)/NavBit.cpp \
    $(SRCDIR)/NavData.cpp \
    $(SRCDIR)/PilotBit.cpp \
    $(SRCDIR)/PowerControl.cpp \
    $(SRCDIR)/PrnGenerate.cpp \
    $(SRCDIR)/Rinex.cpp \
    $(SRCDIR)/SatelliteParam.cpp \
    $(SRCDIR)/SatelliteSignal.cpp \
    $(SRCDIR)/Trajectory.cpp \
    $(SRCDIR)/XmlArguments.cpp \
    $(SRCDIR)/XmlElement.cpp \
    $(SRCDIR)/XmlInterpreter.cpp \
    $(SRCDIR)/FastMath.cpp


# ----------------------------------------------------------------------------
# Source names
# ----------------------------------------------------------------------------

ACTUALIZER_SOURCE = \
    $(ACTUALIZER_DIR)/ActualizeRinex.cpp

ACTUALIZER_MAIN_SOURCE = \
    $(ACTUALIZER_DIR)/RinexActualizerMain.cpp


# ----------------------------------------------------------------------------
# Object names
# ----------------------------------------------------------------------------

SIGNALSIM_OBJECTS = \
    $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SIGNALSIM_SOURCES))

IFDATA_OBJECT = \
    $(OBJDIR)/IFdataGen.o

ACTUALIZER_OBJECT = \
    $(OBJDIR)/ActualizeRinex.o

ACTUALIZER_MAIN_OBJECT = \
    $(OBJDIR)/RinexActualizerMain.o


# ----------------------------------------------------------------------------
# Default target
# ----------------------------------------------------------------------------

all: $(IFDATA_TARGET) $(ACTUALIZER_TARGET)


# ============================================================================
# Directories
# ============================================================================

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)


# ============================================================================
# RINEX Actualizer static library
# ============================================================================

$(LIBDIR)/$(LIB_TARGET): $(ACTUALIZER_OBJECT) | $(LIBDIR)
	@echo "Creating RINEX Actualizer library..."
	ar rcs $@ $<
	@echo "Library created: $@"


# ============================================================================
# IFdataGen
# ============================================================================

$(IFDATA_TARGET): \
	$(IFDATA_OBJECT) \
	$(SIGNALSIM_OBJECTS) \
	$(LIBDIR)/$(LIB_TARGET)

	@echo "Linking $(IFDATA_TARGET)..."

	$(CXX) \
		$(IFDATA_OBJECT) \
		$(SIGNALSIM_OBJECTS) \
		-L$(LIBDIR) \
		-lRinexActualizer \
		$(LDFLAGS) \
		-o $@

	@echo "Build complete: $(IFDATA_TARGET)"


# ============================================================================
# RinexActualizer standalone application
# ============================================================================

$(ACTUALIZER_TARGET): \
	$(ACTUALIZER_MAIN_OBJECT) \
	$(LIBDIR)/$(LIB_TARGET)

	@echo "Linking $(ACTUALIZER_TARGET)..."

	$(CXX) \
		$(ACTUALIZER_MAIN_OBJECT) \
		-L$(LIBDIR) \
		-lRinexActualizer \
		$(LDFLAGS) \
		-o $@

	@echo "Build complete: $(ACTUALIZER_TARGET)"


# ============================================================================
# Compile IFdataGen.cpp
# ============================================================================

$(OBJDIR)/IFdataGen.o: IFdataGen.cpp | $(OBJDIR)

	@echo "Compiling $<..."

	$(CXX) \
		$(CXXFLAGS) \
		-c $< \
		-o $@


# ============================================================================
# Compile SignalSim sources
# ============================================================================

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)

	@echo "Compiling $<..."

	$(CXX) \
		$(CXXFLAGS) \
		-c $< \
		-o $@


# ============================================================================
# Compile ActualizeRinex.cpp
# ============================================================================

$(OBJDIR)/ActualizeRinex.o: \
	$(ACTUALIZER_DIR)/ActualizeRinex.cpp \
	$(ACTUALIZER_DIR)/ActualizeRinex.h | $(OBJDIR)

	@echo "Compiling RINEX Actualizer core..."

	$(CXX) \
		$(CXXFLAGS) \
		-c $(ACTUALIZER_DIR)/ActualizeRinex.cpp \
		-o $@


# ============================================================================
# Compile RinexActualizerMain.cpp
# ============================================================================

$(OBJDIR)/RinexActualizerMain.o: \
	$(ACTUALIZER_DIR)/RinexActualizerMain.cpp \
	$(ACTUALIZER_DIR)/ActualizeRinex.h | $(OBJDIR)

	@echo "Compiling RINEX Actualizer application..."

	$(CXX) \
		$(CXXFLAGS) \
		-c $(ACTUALIZER_DIR)/RinexActualizerMain.cpp \
		-o $@


# ============================================================================
# Clean
# ============================================================================

clean:
	@echo "Cleaning..."

	rm -rf $(OBJDIR)
	rm -rf $(LIBDIR)
	rm -f $(IFDATA_TARGET)
	rm -f $(ACTUALIZER_TARGET)

	@echo "Clean complete."


# ============================================================================
# Rebuild
# ============================================================================

rebuild: clean all


# ============================================================================
# Run IFdataGen
# ============================================================================

run-ifdatagen: $(IFDATA_TARGET)

	./$(IFDATA_TARGET)


# ============================================================================
# Run RinexActualizer
# ============================================================================

run-actualizer: $(ACTUALIZER_TARGET)

	./$(ACTUALIZER_TARGET)


# ============================================================================
# Test
# ============================================================================

test: all

	@echo ""
	@echo "============================================"
	@echo "Build test"
	@echo "============================================"

	@test -f $(IFDATA_TARGET) \
		&& echo "IFdataGen: OK" \
		|| echo "IFdataGen: FAILED"

	@test -f $(ACTUALIZER_TARGET) \
		&& echo "RinexActualizerApp: OK" \
		|| echo "RinexActualizerApp: FAILED"

	@test -f $(LIBDIR)/$(LIB_TARGET) \
		&& echo "RinexActualizer library: OK" \
		|| echo "RinexActualizer library: FAILED"


# ============================================================================
# Help
# ============================================================================

help:

	@echo ""
	@echo "Sisi GNSS Tools"
	@echo ""
	@echo "Targets:"
	@echo "  make                 Build everything"
	@echo "  make clean           Remove build files"
	@echo "  make rebuild         Clean + build"
	@echo "  make test            Check build outputs"
	@echo "  make run-ifdatagen   Run IFdataGen"
	@echo "  make run-actualizer  Run RinexActualizer"
	@echo ""
	@echo "Compiler:"
	@echo "  CXX=g++"
	@echo "  CXXFLAGS=$(CXXFLAGS)"
	@echo ""


# ============================================================================
# Phony
# ============================================================================

.PHONY: \
	all \
	clean \
	rebuild \
	test \
	run-ifdatagen \
	run-actualizer \
	help