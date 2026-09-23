# IFDataGen – GNSS IF adatgenerátor

Nagy teljesítményű GNSS közbenső frekvenciás (IF) jelgenerátor
GPS, Galileo, BDS és GLONASS rendszerekhez.

Az IFDataGen használható:

* standalone parancssori generátorként,
* C++ library-ként,
* C API-n keresztül,
* programozott paraméterekkel JSON fájl nélkül,
* közvetlen IQ streaminggel.

A library használatakor **nem szükséges JSON konfigurációs fájl**.
A `GNSSConfig` struktúrával közvetlenül C++ kódból állítható össze
a teljes szimulációs konfiguráció.

---

## Funkciók

* **Több csillagkép**: GPS, Galileo, BDS, GLONASS
* **Több jel**:

  * GPS: L1CA, L1C, L2C, L2P, L5
  * Galileo: E1, E5a, E5b, E5, E6
  * BDS: B1I, B1C, B2I, B3I, B2a, B2b
  * GLONASS: G1, G2, G3
* **RINEX 3.x / 4.x** navigációs fájl támogatás
* **Dinamikus pálya**:

  * állandó sebesség
  * gyorsulás
  * vertikális gyorsulás
  * jerk
  * vízszintes kanyar
* **Valósághű modellezés**:

  * Doppler
  * ionoszférikus késleltetés
  * troposzférikus késleltetés
* **Kimeneti formátumok**:

  * IQ2
  * IQ4
  * IQ8
  * IQ16
* **OpenMP párhuzamosítás**
* **JSON konfiguráció**
* **Programozott C++ konfiguráció JSON nélkül**
* **Közvetlen IQ sample streaming**
* **Callback-alapú streaming**
* **Chunk-alapú `read()` API**
* **C és C++ API**
* **Standalone és library használat**

---

# Architektúra

A library konfigurációs útja a következő:

```text
                    ┌──────────────────────┐
                    │   C++ application    │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │    GNSSGenerator     │
                    └──────────┬───────────┘
                               │
                 ┌─────────────┴─────────────┐
                 │                           │
                 ▼                           ▼
        GNSSConfig                  JSON configuration
                 │                           │
                 ▼                           ▼
          IfGenParams                  IfGenOptions
                 │                           │
                 └─────────────┬─────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │     IfGenEngine      │
                    └──────────┬───────────┘
                               │
                               ▼
                         SignalSim
```

A fontos változás:

> A JSON most már csak egy konfigurációs bemenet. Nem a library működésének feltétele.

Programozott használatkor:

```text
GNSSConfig
    ↓
IfGenParams
    ↓
IfGenEngine
    ↓
SignalSim
```

Ez lehetővé teszi például azt is, hogy egy külső rendszer saját konfigurációs formátumát használja:

```text
MQTT
  │
  ▼
saját konfiguráció
  │
  ▼
GNSSConfig / IfGenParams
  │
  ▼
GNSSGenerator / IfGenEngine
```

Így az alkalmazásnak nem kell ideiglenesen JSON fájlt létrehoznia csak azért,
hogy a generátort elindítsa.

---

# Követelmények

## Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ gcc make cmake libomp-dev
```

---

# Windows

A projekt **CMake-alapú**, ezért Windows alatt nem a Makefile-t,
hanem a `CMakeLists.txt`-t kell használni.

A Makefile POSIX eszközökre (`make`, `ar`, `uname`) épít, amelyek
Windows alatt csak MSYS2 / Cygwin környezetből érhetők el kényelmesen.

A `CMakeLists.txt` MSVC-vel és Ninja-val használható.

## Szükséges komponensek

* **Visual Studio 2022** (Community is megfelelő)
* *Desktop development with C++*

  * *C++ CMake tools for Windows*
  * *MSVC v143 – VS 2022 C++ x64/x86 build tools*
* **CMake ≥ 3.16**
* **Ninja**

### OpenMP

Az MSVC saját OpenMP implementációt tartalmaz.

A `find_package(OpenMP)` automatikusan megpróbálja megtalálni.
Ha nem érhető el, a build OpenMP nélkül is elkészíthető,
egyszálú működéssel.

---

# Fordítás Linux / Ubuntu alatt

## A) CMake + Make

```bash
git clone <your-repo-url>
cd Sisi/GNSSLib

./build.sh
```

A `build.sh` környezeti változói:

| Változó          | Alapérték    | Leírás                       |
| ---------------- | ------------ | ---------------------------- |
| `BUILD_TYPE`     | `Release`    | CMake build típus            |
| `BUILD_DIR`      | `build`      | Build könyvtár               |
| `INSTALL_PREFIX` | `/usr/local` | Telepítési prefix            |
| `JOBS`           | `nproc`      | Párhuzamos jobok             |
| `DO_INSTALL`     | `1`          | Telepítés                    |
| `DO_TEST`        | `1`          | Example futtatása            |
| `USE_NATIVE_OPT` | `ON`         | CPU-specifikus optimalizáció |

Például Debug build:

```bash
BUILD_TYPE=Debug USE_NATIVE_OPT=OFF DO_INSTALL=0 ./build.sh
```

---

## B) Kézi CMake

```bash
mkdir -p build
cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DGNSSLIB_BUILD_SHARED=ON \
    -DGNSSLIB_BUILD_STATIC=ON \
    -DGNSSLIB_BUILD_EXAMPLES=ON \
    -DGNSSLIB_BUILD_TOOLS=ON

make -j$(nproc)
```

A kimenetek:

```text
build/lib/libGNSSLib.so
build/lib/libGNSSLib.a
build/bin/IFdataGen
build/bin/RinexActualizerApp
build/bin/basic_usage
```

---

# Fordítás Windows / Visual Studio alatt

## A) Visual Studio

1. Klónozd a repót.
2. Visual Studio → **File → Open → Folder**
3. Válaszd a `Sisi\GNSSLib` könyvtárat.
4. Válaszd ki például az `x64-Release` konfigurációt.
5. **Build → Build All**

A binárisok például:

```text
out\build\x64-Release\bin\
```

---

## B) Ninja

Developer PowerShell:

```powershell
cd Sisi\GNSSLib

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Teszt:

```powershell
.\build\bin\basic_usage.exe
```

---

## C) Visual Studio solution

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

# Fontos build opciók

## AVX2 / native optimalizáció kikapcsolása

Ha a build géptől eltérő CPU-kon szeretnéd futtatni:

```powershell
cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_NATIVE_OPT=OFF
```

Linuxon:

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_NATIVE_OPT=OFF
```

## Csak a library

```powershell
cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DGNSSLIB_BUILD_TOOLS=OFF
```

---

# Használat standalone utilityként

A standalone generátor továbbra is használható JSON konfigurációval:

```bash
./build/bin/IFdataGen -c configs/example.json
```

A JSON-os használat tehát nem szűnt meg.

A változás az, hogy **library használatakor már nem szükséges JSON**.

---

# Library használat CMake-ből

A saját CMake projektedben:

```cmake
find_package(GNSSLib REQUIRED)

target_link_libraries(
    myapp
    PRIVATE
        GNSSLib::GNSSLib
)
```

Ezután:

```cpp
#include <gnss/gnsslib.h>
```

---

# C++ API – programozott konfiguráció

Ez a javasolt library API.

Nem szükséges JSON fájl.

```cpp
#include <gnss/gnsslib.h>

#include <iostream>

int main()
{
    gnss::GNSSGenerator generator;

    gnss::GNSSConfig config;

    // Simulation time
    config.time.year   = 2021;
    config.time.month  = 6;
    config.time.day    = 19;
    config.time.hour   = 12;
    config.time.minute = 0;
    config.time.second = 0.0;

    // Receiver position
    config.position.latitude  = 47.492721;
    config.position.longitude = 19.015773;
    config.position.altitude  = 120.0;

    // Receiver velocity
    config.velocity.speed  = 10.0;
    config.velocity.course = 90.0;

    // Navigation file
    config.ephemerisFile =
        "BRDC00WRD_R_20211700000_01D_GN.rnx";

    // RINEX actualization
    config.actualizeRinex = true;

    // Output
    config.output.format = gnss::OutputFormat::IQ16;
    config.output.sampleRate = 4.0e6;
    config.output.centerFreq = 1575.42e6;
    config.output.filename = "output.bin";

    // Power
    config.power.noiseFloor = -172.0;
    config.power.initPower = 47.0;
    config.power.elevationAdjust = true;

    // Signals
    config.signals.push_back({
        gnss::SystemType::GPS,
        gnss::SignalType::GPS_L1CA,
        true
    });

    config.signals.push_back({
        gnss::SystemType::Galileo,
        gnss::SignalType::GAL_E1,
        true
    });

    if (!generator.initialize(config)) {
        std::cerr << "Initialization failed\n";
        return 1;
    }

    if (!generator.generate(10.0)) {
        std::cerr << "Generation failed\n";
        return 1;
    }

    return 0;
}
```

---

# Mi történik a háttérben?

A fenti hívás:

```cpp
generator.initialize(config);
```

nem hoz létre JSON fájlt.

A library belül ezt csinálja:

```text
GNSSConfig
    │
    ▼
IfGenParams
    │
    ▼
IfGenEngine::Init(params)
    │
    ▼
SignalSim
```

Ez ugyanazt az engine/setup útvonalat használja, amelyet a
közvetlen `IfGenParams` API is használ.

---

# C++ API – streaming JSON nélkül

Ha nem fájlba szeretnéd írni a teljes generált jelet, hanem az IQ mintákat
közvetlenül a saját alkalmazásodban szeretnéd feldolgozni:

```cpp
#include <gnss/gnsslib.h>

#include <complex>
#include <vector>

int main()
{
    gnss::GNSSGenerator generator;

    gnss::GNSSConfig config;

    config.time.year   = 2021;
    config.time.month  = 6;
    config.time.day    = 19;

    config.position.latitude  = 47.49;
    config.position.longitude = 19.01;
    config.position.altitude  = 120.0;

    config.ephemerisFile =
        "navigation.rnx";

    config.output.sampleRate =
        4.0e6;

    config.output.centerFreq =
        1575.42e6;

    config.output.format =
        gnss::OutputFormat::IQ16;

    config.signals.push_back({
        gnss::SystemType::GPS,
        gnss::SignalType::GPS_L1CA,
        true
    });

    if (!generator.initialize(config)) {
        return 1;
    }

    /*
     * Open streaming mode.
     *
     * No JSON file is required.
     */
    if (!generator.openStream()) {
        return 1;
    }

    std::vector<std::complex<float>> buffer(4096);

    while (true) {

        const std::size_t n =
            generator.read(
                buffer.data(),
                buffer.size()
            );

        if (n == 0) {
            break;
        }

        /*
         * Process n IQ samples.
         */
        // process(buffer.data(), n);
    }

    generator.closeStream();

    return 0;
}
```

---

# Callback alapú streaming

Ha nincs szükség saját `read()` ciklusra, használható callback is:

```cpp
if (!generator.openStream()) {
    return 1;
}

generator.stream(
    [](const std::complex<float>* samples,
       std::size_t count)
    {
        /*
         * Process samples.
         */

        // process(samples, count);

        /*
         * true  -> continue
         * false -> stop
         */
        return true;
    },
    4096
);

generator.closeStream();
```

A callback minden híváskor egy contiguous IQ sample blokkot kap.

---

# Streaming és fájlba írás egyszerre

A stream megnyitható úgy is, hogy a library a mintákat a fogyasztónak
is továbbítja, miközben binary output is készül:

```cpp
generator.openStream(
    true,
    "output.bin",
    1.0f
);
```

Paraméterek:

```text
writeBin
    true  -> binary output is enabled

binFile
    output binary file

gain
    signal gain
```

---

# IfGenEngine közvetlen használata

Az alacsonyabb szintű API továbbra is elérhető.

```cpp
#include <gnss/IfGenEngine.h>

gnss::IfGenParams params;

/*
 * Fill params...
 */

gnss::IfGenEngine engine;

engine.Init(params);

engine.Stream(
    [](const std::complex<float>* samples,
       std::size_t count)
    {
        // process samples
        return true;
    }
);
```

A különbség:

```text
GNSSGenerator
    ↑
magasabb szintű library API

IfGenEngine
    ↑
alacsonyabb szintű engine API
```

A legtöbb library-felhasználónak a `GNSSGenerator` használata javasolt.

---

# JSON API

A JSON konfiguráció továbbra is támogatott.

```cpp
#include <gnss/gnsslib.h>

gnss::GNSSGenerator generator;

if (!generator.initializeFromJSON(
        "config.json")) {
    return 1;
}

generator.generate(10.0);
```

A JSON API elsősorban:

* standalone használathoz,
* konfigurációs fájl alapú futtatáshoz,
* meglévő workflow-k kompatibilitásához

hasznos.

Új library-integráció esetén nem kötelező használni.

---

# C API

A C API továbbra is használható JSON-alapú konfigurációval:

```c
#include <gnss/gnsslib.h>

GNSSGeneratorHandle* gen =
    gnss_create_generator();

if (gnss_init_from_json(
        gen,
        "config.json")) {

    gnss_generate(
        gen,
        10.0
    );

    gnss_save(
        gen,
        "output.bin"
    );

    double duration;
    double sizeMB;

    gnss_get_stats(
        gen,
        &duration,
        &sizeMB
    );
}

gnss_destroy_generator(gen);
```

A programozott konfiguráció elsődleges új API-ja jelenleg a C++ API,
mert a teljes `GNSSConfig` struktúra C-ben történő leképezése jelentősen
nagyobb interfészt eredményezne.

---

# C API streaming

Az alacsony szintű C streaming API:

```c
gnss_stream_options options;

gnss_stream_options_default(&options);

options.config_file =
    "config.json";

options.write_bin = 0;

gnss_stream* stream =
    gnss_stream_open(&options);

if (stream == NULL) {
    return 1;
}

char buffer[4096 * 2];

while (1) {

    size_t n =
        gnss_stream_read(
            stream,
            buffer,
            4096
        );

    if (n == 0) {
        break;
    }

    /*
     * Process samples.
     */
}

gnss_stream_close(stream);
```

---

# Library telepítése

Linux alatt a `build.sh` alapértelmezésben telepíti:

```text
/usr/local/lib/libGNSSLib.so
/usr/local/lib/libGNSSLib.a
/usr/local/include/gnss/*.h
/usr/local/lib/cmake/GNSSLib/*
/usr/local/lib/pkgconfig/gnsslib.pc
```

Eltávolítás:

```bash
cd build
sudo xargs rm < install_manifest.txt
```

---

# Másik CMake projektből

Telepítés után:

```cmake
find_package(GNSSLib REQUIRED)

add_executable(my_gnss_app
    main.cpp
)

target_link_libraries(
    my_gnss_app
    PRIVATE
        GNSSLib::GNSSLib
)
```

A programban:

```cpp
#include <gnss/gnsslib.h>
```

---

# pkg-config

Linuxon:

```bash
g++ myapp.cpp \
    $(pkg-config --cflags --libs gnsslib) \
    -o myapp
```

---

# API rétegek

A library három különböző használati szintet biztosít:

```text
┌──────────────────────────────────────────┐
│ GNSSGenerator                            │
│                                          │
│ - GNSSConfig                             │
│ - initialize(config)                     │
│ - initializeFromJSON()                   │
│ - generate()                             │
│ - openStream()                           │
│ - read()                                 │
│ - stream()                               │
└───────────────────┬──────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────┐
│ IfGenEngine                              │
│                                          │
│ - IfGenParams                            │
│ - IfGenOptions                           │
│ - Init(params)                           │
│ - Init(options)                          │
│ - Read()                                 │
│ - Stream()                               │
└───────────────────┬──────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────┐
│ SignalSim                                │
│                                          │
│ GNSS signal generation                   │
└──────────────────────────────────────────┘
```

A `GNSSGenerator` célja, hogy az alkalmazásnak ne kelljen
SignalSim-internals, JSON parsing vagy `IfGenOptions` kezelésével
foglalkoznia.

---

# Ajánlott használat új alkalmazásokhoz

Új C++ alkalmazás esetén:

```cpp
gnss::GNSSGenerator generator;

gnss::GNSSConfig config;

/*
 * Configure simulation...
 */

generator.initialize(config);

generator.openStream();

generator.stream(
    [](const std::complex<float>* samples,
       std::size_t count)
    {
        /*
         * Application-specific processing.
         */
        return true;
    }
);

generator.closeStream();
```

Ebben a használati módban:

* nincs JSON fájl,
* nincs ideiglenes konfigurációs fájl,
* nincs közvetlen SignalSim dependency a fogyasztó kódjában,
* a konfiguráció C++ struktúrákból épül fel,
* a generált IQ adatok közvetlenül streamelhetők,
* a `GNSSGenerator` kezeli az `IfGenEngine` életciklusát.

---

# Tervezési elv

A konfigurációs forrás és a generációs engine szét van választva.

Lehetséges bemenetek:

```text
JSON ────────┐
             │
MQTT ────────┤
             ├──→ GNSSConfig / IfGenParams
SCG ─────────┤             │
             │             ▼
C++ API ─────┘       IfGenEngine
                           │
                           ▼
                        SignalSim
```

A JSON ezért nem a generátor belső működésének része,
hanem csak az egyik lehetséges konfigurációs adapter.

---

# Ismert buktatók Windows / MSVC alatt

## UTF-8

A forrás UTF-8 kódolású.

A `CMakeLists.txt` beállítja az `/utf-8` MSVC kapcsolót.

## `M_PI`

MSVC alatt a matematikai konstansokhoz szükséges lehet:

```cpp
#define _USE_MATH_DEFINES
#include <cmath>
```

A projekt CMake konfigurációja ezt kezeli.

## `/permissive-`

A projekt szigorúbb ISO C++ megfelelést használ.

Ha saját buildben problémát okoz, ellenőrizd a Release /
RelWithDebInfo fordítási opciókat.

## `/bigobj`

A nagy template-heavy fordításokhoz szükséges lehet.

A projekt CMake konfigurációja tartalmazza.

## AVX2

Ha a build gépen fordított binary más CPU-kon is fut, ajánlott:

```text
USE_NATIVE_OPT=OFF
```

## POSIX fejlécek

Ha a `SignalSim/src` vagy a `RinexActualizer` forrásai POSIX-specifikus
fejléceket használnak, például:

```text
unistd.h
dirent.h
sys/time.h
```

akkor ezek Windows-kompatibilitását forrásszinten kell biztosítani.

A CMake önmagában ezt nem oldja meg.

---

# Licenc

A projekt a SignalSim / RinexActualizer eredeti licence alatt áll.

Lásd a `LICENSE` fájlt a részletekért.

Copyright (C) 2020–2029 by Jun Mo, All rights reserved.

Kiegészítve és library-vé alakítva.
