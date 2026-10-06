project(
    'ssp',
    'cpp',
    version: '1.0.0',
    default_options: [
        'cpp_std=c++20',
        # SignalSim and RinexActualizer are upstream code taken as it comes, so the
        # warnings of whoever consumes this library are not applied to it
        'warning_level=0',
    ],
    meson_version: '>=1.1'
)

compiler = meson.get_compiler('cpp')

# the engine parallelises the per satellite work with openmp. without it the sources
# still build, single threaded, which is what the upstream cmake does as well
openmpDep = dependency('openmp', required: false)
threadsDep = dependency('threads')

gnssDefines = []
if openmpDep.found()
    gnssDefines += '-DUSE_OPENMP'
else
    warning('openmp not found, the gnss engine will run single threaded')
endif

# PrnGenerate.cpp includes MemoryCode.dat out of its own directory, which the quoted
# form of an include finds without src being on the path
signalSimInc = include_directories('SignalSim/inc')
rinexInc = include_directories('RinexActualizer')
gnssLibInc = include_directories('GNSSLib/include')

signalSimSources = files(
    'SignalSim/src/Almanac.cpp',
    'SignalSim/src/BCNav1Bit.cpp',
    'SignalSim/src/BCNav2Bit.cpp',
    'SignalSim/src/BCNav3Bit.cpp',
    'SignalSim/src/BCNavBit.cpp',
    'SignalSim/src/CNav2Bit.cpp',
    'SignalSim/src/CNavBit.cpp',
    'SignalSim/src/ComplexNumber.cpp',
    'SignalSim/src/Coordinate.cpp',
    'SignalSim/src/D1D2NavBit.cpp',
    'SignalSim/src/DelayModel.cpp',
    'SignalSim/src/FNavBit.cpp',
    'SignalSim/src/FastMath.cpp',
    'SignalSim/src/GNavBit.cpp',
    'SignalSim/src/GnssTime.cpp',
    'SignalSim/src/INavBit.cpp',
    'SignalSim/src/JsonComposer.cpp',
    'SignalSim/src/JsonInterpreter.cpp',
    'SignalSim/src/JsonParser.cpp',
    'SignalSim/src/LNavBit.cpp',
    'SignalSim/src/MessageOutput.cpp',
    'SignalSim/src/NavBit.cpp',
    'SignalSim/src/NavData.cpp',
    'SignalSim/src/PilotBit.cpp',
    'SignalSim/src/PowerControl.cpp',
    'SignalSim/src/PrnGenerate.cpp',
    'SignalSim/src/Rinex.cpp',
    'SignalSim/src/SatIfSignal.cpp',
    'SignalSim/src/SatelliteParam.cpp',
    'SignalSim/src/SatelliteSignal.cpp',
    'SignalSim/src/Trajectory.cpp',
    'SignalSim/src/XmlArguments.cpp',
    'SignalSim/src/XmlElement.cpp',
    'SignalSim/src/XmlInterpreter.cpp',
)

gnssLibSources = files(
    'RinexActualizer/ActualizeRinex.cpp',
    'GNSSLib/src/GNSSGenerator.cpp',
    'GNSSLib/src/IfGenEngine.cpp',
)

gnssLib = static_library(
    'gnsslib',
    sources: signalSimSources + gnssLibSources,
    include_directories: [gnssLibInc, signalSimInc, rinexInc],
    cpp_args: gnssDefines,
    dependencies: [openmpDep, threadsDep],
)

# only the GNSSLib headers are handed out, the ones under SignalSim are an implementation
# detail of it
sspDep = declare_dependency(
    link_with: gnssLib,
    include_directories: gnssLibInc,
    dependencies: [openmpDep, threadsDep],
)

meson.override_dependency('ssp', sspDep)
