# ============================================================================
# Sisi GNSS Tools - Extended Makefile
#
# Builds:
#   1. IFdataGen (standalone utility)
#   2. RinexActualizerApp (standalone utility)
#   3. libGNSSLib.so (shared library)
#   4. libGNSSLib.a (static library)
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
AR  ?= ar

# ----------------------------------------------------------------------------
# Targets
# ----------------------------------------------------------------------------

IFDATA_TARGET = IFdataGen
ACTUALIZER_TARGET = RinexActualizerApp
LIB_SHARED_TARGET = libGNSSLib.so
LIB_STATIC_TARGET = libGNSSLib.a
LIB_ACTUALIZER_TARGET = libRinexActualizer.a

# Library version
LIB_VERSION = 1.0.0
LIB_SOVERSION = 1

# ----------------------------------------------------------------------------
# Directories
# ----------------------------------------------------------------------------

SRCDIR = ../src
INCDIR = ../inc

ACTUALIZER_DIR = ../../RinexActualizer

OBJDIR = obj
LIBDIR = lib
BINDIR = .
INSTALL_PREFIX ?= /usr/local

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
# Position Independent Code (for shared library)
# ----------------------------------------------------------------------------

PIC_FLAGS = -fPIC

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
    $(SRCDIR)/DelayModel.cpp \
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
    $(SRCDIR)/MessageOutput.cpp \
    $(SRCDIR)/FastMath.cpp

# ----------------------------------------------------------------------------
# Source names
# ----------------------------------------------------------------------------

ACTUALIZER_SOURCE = \
    $(ACTUALIZER_DIR)/ActualizeRinex.cpp

ACTUALIZER_MAIN_SOURCE = \
    $(ACTUALIZER_DIR)/RinexActualizerMain.cpp

IFDATA_SOURCE = \
    IFdataGen.cpp

# ----------------------------------------------------------------------------
# Object names
# ----------------------------------------------------------------------------

# Standard objects (without -fPIC)
SIGNALSIM_OBJECTS = \
    $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SIGNALSIM_SOURCES))

IFDATA_OBJECT = \
    $(OBJDIR)/IFdataGen.o

ACTUALIZER_OBJECT = \
    $(OBJDIR)/ActualizeRinex.o

ACTUALIZER_MAIN_OBJECT = \
    $(OBJDIR)/RinexActualizerMain.o

# PIC objects (for shared library)
SIGNALSIM_PIC_OBJECTS = \
    $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/pic/%.pic.o,$(SIGNALSIM_SOURCES))

ACTUALIZER_PIC_OBJECT = \
    $(OBJDIR)/pic/ActualizeRinex.pic.o

# ----------------------------------------------------------------------------
# Default target
# ----------------------------------------------------------------------------

all: $(IFDATA_TARGET) $(ACTUALIZER_TARGET) $(LIB_SHARED_TARGET) $(LIB_STATIC_TARGET)

# ----------------------------------------------------------------------------
# Help message
# ----------------------------------------------------------------------------

help:
	@echo ""
	@echo "Sisi GNSS Tools - Extended Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make all                 Build everything (standalone + libraries)"
	@echo "  make IFdataGen           Build standalone IFdataGen"
	@echo "  make RinexActualizerApp  Build standalone RinexActualizer"
	@echo "  make lib                 Build shared and static libraries"
	@echo "  make lib-shared          Build shared library only"
	@echo "  make lib-static          Build static library only"
	@echo "  make clean               Remove build files"
	@echo "  make rebuild             Clean + rebuild all"
	@echo "  make install             Install libraries and headers"
	@echo "  make test                Check build outputs"
	@echo "  make run-ifdatagen       Run IFdataGen"
	@echo "  make run-actualizer      Run RinexActualizer"
	@echo ""
	@echo "Compiler:"
	@echo "  CXX=$(CXX)"
	@echo "  CXXFLAGS=$(CXXFLAGS)"
	@echo ""
	@echo "Library version: $(LIB_VERSION)"
	@echo "Install prefix: $(INSTALL_PREFIX)"
	@echo ""

# ============================================================================
# Directories
# ============================================================================

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/pic

$(LIBDIR):
	mkdir -p $(LIBDIR)

# ============================================================================
# RINEX Actualizer static library (used by standalone)
# ============================================================================

$(LIBDIR)/$(LIB_ACTUALIZER_TARGET): $(ACTUALIZER_OBJECT) | $(LIBDIR)
	@echo "Creating RINEX Actualizer library..."
	$(AR) rcs $@ $<
	@echo "Library created: $@"

# ============================================================================
# IFdataGen (Standalone)
# ============================================================================

$(IFDATA_TARGET): \
	$(IFDATA_OBJECT) \
	$(SIGNALSIM_OBJECTS) \
	$(LIBDIR)/$(LIB_ACTUALIZER_TARGET)

	@echo "Linking $(IFDATA_TARGET) (standalone)..."
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
	$(LIBDIR)/$(LIB_ACTUALIZER_TARGET)

	@echo "Linking $(ACTUALIZER_TARGET) (standalone)..."
	$(CXX) \
		$(ACTUALIZER_MAIN_OBJECT) \
		-L$(LIBDIR) \
		-lRinexActualizer \
		$(LDFLAGS) \
		-o $@
	@echo "Build complete: $(ACTUALIZER_TARGET)"

# ============================================================================
# Shared Library (libGNSSLib.so)
# ============================================================================

lib-shared: $(LIB_SHARED_TARGET)

$(LIB_SHARED_TARGET): \
	$(SIGNALSIM_PIC_OBJECTS) \
	$(ACTUALIZER_PIC_OBJECT)

	@echo "Building shared library $(LIB_SHARED_TARGET)..."
	$(CXX) -shared \
		-Wl,-soname,$(LIB_SHARED_TARGET).$(LIB_SOVERSION) \
		$(SIGNALSIM_PIC_OBJECTS) \
		$(ACTUALIZER_PIC_OBJECT) \
		$(LDFLAGS) \
		-o $(LIBDIR)/$@
	@echo "Creating symlinks..."
	cd $(LIBDIR) && ln -sf $(LIB_SHARED_TARGET) $(LIB_SHARED_TARGET).$(LIB_SOVERSION)
	cd $(LIBDIR) && ln -sf $(LIB_SHARED_TARGET).$(LIB_SOVERSION) $(LIB_SHARED_TARGET).$(LIB_VERSION)
	@echo "Shared library created: $(LIBDIR)/$(LIB_SHARED_TARGET)"

# ============================================================================
# Static Library (libGNSSLib.a)
# ============================================================================

lib-static: $(LIB_STATIC_TARGET)

$(LIB_STATIC_TARGET): \
	$(SIGNALSIM_OBJECTS) \
	$(ACTUALIZER_OBJECT)

	@echo "Building static library $(LIB_STATIC_TARGET)..."
	$(AR) rcs $(LIBDIR)/$@ $^
	@echo "Static library created: $(LIBDIR)/$(LIB_STATIC_TARGET)"

# ============================================================================
# Libraries target (builds both shared and static)
# ============================================================================

lib: $(LIB_SHARED_TARGET) $(LIB_STATIC_TARGET)

# ============================================================================
# Compile IFdataGen.cpp
# ============================================================================

$(OBJDIR)/IFdataGen.o: IFdataGen.cpp | $(OBJDIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Compile SignalSim sources (standard)
# ============================================================================

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Compile SignalSim sources (PIC for shared library)
# ============================================================================

$(OBJDIR)/pic/%.pic.o: $(SRCDIR)/%.cpp | $(OBJDIR)/pic
	@echo "Compiling $< (PIC)..."
	$(CXX) $(CXXFLAGS) $(PIC_FLAGS) -c $< -o $@

# ============================================================================
# Compile ActualizeRinex.cpp (standard)
# ============================================================================

$(OBJDIR)/ActualizeRinex.o: \
	$(ACTUALIZER_DIR)/ActualizeRinex.cpp \
	$(ACTUALIZER_DIR)/ActualizeRinex.h | $(OBJDIR)

	@echo "Compiling RINEX Actualizer core..."
	$(CXX) $(CXXFLAGS) -c $(ACTUALIZER_DIR)/ActualizeRinex.cpp -o $@

# ============================================================================
# Compile ActualizeRinex.cpp (PIC for shared library)
# ============================================================================

$(OBJDIR)/pic/ActualizeRinex.pic.o: \
	$(ACTUALIZER_DIR)/ActualizeRinex.cpp \
	$(ACTUALIZER_DIR)/ActualizeRinex.h | $(OBJDIR)/pic

	@echo "Compiling RINEX Actualizer core (PIC)..."
	$(CXX) $(CXXFLAGS) $(PIC_FLAGS) -c $(ACTUALIZER_DIR)/ActualizeRinex.cpp -o $@

# ============================================================================
# Compile RinexActualizerMain.cpp
# ============================================================================

$(OBJDIR)/RinexActualizerMain.o: \
	$(ACTUALIZER_DIR)/RinexActualizerMain.cpp \
	$(ACTUALIZER_DIR)/ActualizeRinex.h | $(OBJDIR)

	@echo "Compiling RINEX Actualizer application..."
	$(CXX) $(CXXFLAGS) -c $(ACTUALIZER_DIR)/RinexActualizerMain.cpp -o $@

# ============================================================================
# Install
# ============================================================================

install: all
	@echo "Installing..."
	@echo "  Installing standalone utilities..."
	install -d $(DESTDIR)$(INSTALL_PREFIX)/bin
	install -m 755 $(IFDATA_TARGET) $(DESTDIR)$(INSTALL_PREFIX)/bin/
	install -m 755 $(ACTUALIZER_TARGET) $(DESTDIR)$(INSTALL_PREFIX)/bin/
	@echo "  Installing libraries..."
	install -d $(DESTDIR)$(INSTALL_PREFIX)/lib
	install -m 644 $(LIBDIR)/$(LIB_SHARED_TARGET)* $(DESTDIR)$(INSTALL_PREFIX)/lib/
	install -m 644 $(LIBDIR)/$(LIB_STATIC_TARGET) $(DESTDIR)$(INSTALL_PREFIX)/lib/
	install -m 644 $(LIBDIR)/$(LIB_ACTUALIZER_TARGET) $(DESTDIR)$(INSTALL_PREFIX)/lib/
	@echo "  Installing headers..."
	install -d $(DESTDIR)$(INSTALL_PREFIX)/include/gnss
	install -m 644 $(INCDIR)/*.h $(DESTDIR)$(INSTALL_PREFIX)/include/gnss/ 2>/dev/null || true
	install -m 644 $(ACTUALIZER_DIR)/ActualizeRinex.h $(DESTDIR)$(INSTALL_PREFIX)/include/gnss/
	@echo "  Creating library symlinks..."
	cd $(DESTDIR)$(INSTALL_PREFIX)/lib && ldconfig 2>/dev/null || true
	@echo "Install complete."

# ============================================================================
# Uninstall
# ============================================================================

uninstall:
	@echo "Uninstalling..."
	rm -f $(DESTDIR)$(INSTALL_PREFIX)/bin/$(IFDATA_TARGET)
	rm -f $(DESTDIR)$(INSTALL_PREFIX)/bin/$(ACTUALIZER_TARGET)
	rm -f $(DESTDIR)$(INSTALL_PREFIX)/lib/$(LIB_SHARED_TARGET)*
	rm -f $(DESTDIR)$(INSTALL_PREFIX)/lib/$(LIB_STATIC_TARGET)
	rm -f $(DESTDIR)$(INSTALL_PREFIX)/lib/$(LIB_ACTUALIZER_TARGET)
	rm -rf $(DESTDIR)$(INSTALL_PREFIX)/include/gnss
	@echo "Uninstall complete."

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
	@test -f $(LIBDIR)/$(LIB_SHARED_TARGET) \
		&& echo "Shared library: OK" \
		|| echo "Shared library: FAILED"
	@test -f $(LIBDIR)/$(LIB_STATIC_TARGET) \
		&& echo "Static library: OK" \
		|| echo "Static library: FAILED"
	@test -f $(LIBDIR)/$(LIB_ACTUALIZER_TARGET) \
		&& echo "RinexActualizer library: OK" \
		|| echo "RinexActualizer library: FAILED"

# ============================================================================
# Phony
# ============================================================================

.PHONY: \
	all \
	lib \
	lib-shared \
	lib-static \
	clean \
	rebuild \
	install \
	uninstall \
	test \
	run-ifdatagen \
	run-actualizer \
	help