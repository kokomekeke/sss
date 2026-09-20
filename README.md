# IFDataGen – GNSS IF adatgenerátor

Nagy teljesítményű GNSS közbenső frekvenciás (IF) jelgenerátor
GPS, Galileo, BDS és GLONASS rendszerekhez.

## Funkciók

- **Több csillagkép**: GPS, Galileo, BDS, GLONASS
- **Több jel**: L1CA, L1C, L2C, L5 (GPS); E1, E5a, E5b (Galileo);
  B1I, B1C (BDS); G1, G2 (GLONASS)
- **RINEX 3.x / 4.x** navigációs fájl támogatás
- **Dinamikus pálya**: állandó sebesség, gyorsulás, kanyarok, vertikális mozgás
- **Valósághű modellezés**: Doppler, ionoszférikus / troposzférikus késleltetés
- **Kimeneti formátumok**: IQ8, IQ16
- **OpenMP párhuzamosítás**
- **JSON konfiguráció**

## Követelmények

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ gcc make cmake libomp-dev
```

### Windows

A projekt **CMake-alapú**, ezért Windows alatt **nem a Makefile-t**,
hanem a `CMakeLists.txt`-t kell használni. A Makefile POSIX eszközökre
(`make`, `ar`, `uname`) épít, amelyek Windows alatt csak MSYS2 / Cygwin
környezetből érhetők el kényelmesen. A `CMakeLists.txt` MSVC-vel és
Ninja-val van tesztelve.

Szükséges komponensek:

- **Visual Studio 2022** (Community is megfelelő), a következő
  workload-dal:
  - *Desktop development with C++*
    - *C++ CMake tools for Windows*
    - *MSVC v143 – VS 2022 C++ x64/x86 build tools*
- **CMake ≥ 3.16** (a VS-be integrált CMake is megfelel, külön
  telepítés nem kötelező)
- **Ninja** (a VS „Desktop development with C++” workload automatikusan
  telepíti)

> **OpenMP**: az MSVC saját OpenMP implementációt tartalmaz, a
> `find_package(OpenMP)` automatikusan megtalálja, amennyiben a fenti
> build tools telepítve vannak. Ha mégsem található, a build OpenMP
> nélkül, egyszálas módban fut tovább – ez a CMake konfigurációs
> kimenetben `OpenMP: NOT FOUND` formában jelenik meg.

## Fordítás Linux / Ubuntu alatt

### A) CMake + Make (ajánlott)

```bash
# A repó klónozása
git clone <your-repo-url>
cd Sisi/GNSSLib

# Build script futtatása
./build.sh
```

A `build.sh` a következő környezeti változókat ismeri:

| Változó           | Alapérték      | Leírás |
|-------------------|----------------|--------|
| `BUILD_TYPE`      | `Release`      | CMake build típus |
| `BUILD_DIR`       | `build`        | Build könyvtár |
| `INSTALL_PREFIX`  | `/usr/local`   | Telepítési prefix |
| `JOBS`            | `nproc`        | Párhuzamos jobok |
| `DO_INSTALL`      | `1`            | Telepítés a végén |
| `DO_TEST`         | `1`            | Example futtatása |
| `USE_NATIVE_OPT`  | `ON`           | `-march=native` |

Például Debug build AVX2 nélkül:

```bash
BUILD_TYPE=Debug USE_NATIVE_OPT=OFF DO_INSTALL=0 ./build.sh
```

### B) Kézi CMake

```bash
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DGNSSLIB_BUILD_SHARED=ON \
    -DGNSSLIB_BUILD_STATIC=ON \
    -DGNSSLIB_BUILD_EXAMPLES=ON \
    -DGNSSLIB_BUILD_TOOLS=ON
make -j$(nproc)
```

A kimenetek:

- `build/lib/libGNSSLib.so` – dinamikus library
- `build/lib/libGNSSLib.a` – statikus library
- `build/bin/IFdataGen` – standalone generátor
- `build/bin/RinexActualizerApp` – standalone actualizer
- `build/bin/basic_usage` – példa program

## Fordítás Windows / Visual Studio alatt

### A) Visual Studio grafikus felülete (ajánlott)

1. Klónozd a repót (pl. Developer PowerShell-ből).
2. Visual Studio-ban: **File → Open → Folder…**, és válaszd ki a
   `Sisi\GNSSLib` mappát (itt van a `CMakeLists.txt`).
3. A VS automatikusan felismeri a CMake projektet, és beolvassa a
   mellette lévő `CMakeSettings.json`-t.
4. A felső konfiguráció-választóban válaszd ki pl. az
   `x64-Release`-t.
5. **Build → Build All** (`Ctrl+Shift+B`).
6. A binárisok itt lesznek: `out\build\x64-Release\bin\`.

### B) Parancssorból (Developer PowerShell for VS 2022)

```powershell
cd Sisi\GNSSLib

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Futtatás
.\build\bin\basic_usage.exe
```

Vagy a kényelmi `build.bat` script-tel:

```powershell
.\build.bat
```

### C) Hagyományos Visual Studio solution (.sln)

Ha klasszikus MSBuild workflow-t szeretnél:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Ez létrehoz egy `.sln` fájlt a `build` mappában.

### Fontos build opciók Windows alatt

- **AVX2 kikapcsolása** (ha nem biztos, hogy a célgép CPU-ja támogatja):
  ```powershell
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_NATIVE_OPT=OFF
  ```
- **Csak a library, standalone tool-ok nélkül**:
  ```powershell
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGNSSLIB_BUILD_TOOLS=OFF
  ```

### Ismert buktatók Windows / MSVC alatt

- **UTF-8 kódolás**: a forrás (kommentek) UTF-8-ban vannak, ékezetes
  magyar szöveggel. A `CMakeLists.txt` már beállítja az `/utf-8` MSVC
  kapcsolót – ha saját CMakeLists-t írnál, ne felejtsd el.
- **`M_PI` és társai**: MSVC alatt csak a `_USE_MATH_DEFINES` makróval
  érhetők el – ezt is beállítja a mellékelt fájl.
- **`/permissive-`**: szigorú ISO C++ megfelelést kényszerít ki. Ha a
  build ismeretlen okból olyan kódrészeknél hibázik, amik GCC/Clang
  alatt simán fordulnak, próbáld meg kivenni ezt a kapcsolót a
  Release / RelWithDebInfo ágból.
- **`/bigobj`**: nagy template-objekt fájlokhoz szükséges lehet – már
  be van állítva.
- **AVX2**: ha a build gépen fordítasz, de másik (régebbi) gépen
  futtatnád, kapcsold ki: `-DUSE_NATIVE_OPT=OFF`.
- **Forrás-szintű Windows-kompatibilitás**: ha a `SignalSim/src` vagy
  `RinexActualizer` fájlok POSIX-specifikus fejléceket használnak
  (`<unistd.h>`, `<dirent.h>`, `<sys/time.h>`), azokat forrásszinten
  kell Windows-kompatibilissé tenni – ezt a build rendszer önmagában
  nem oldja meg.

## Használat

### Standalone utility

```bash
./build/bin/IFdataGen -c configs/example.json
```

### Library-ként (CMake)

A `CMakeLists.txt`-ben:

```cmake
find_package(GNSSLib REQUIRED)
target_link_libraries(myapp PRIVATE GNSSLib::GNSSLib)
```

### Library-ként (pkg-config)

```bash
g++ myapp.cpp $(pkg-config --cflags --libs gnsslib) -o myapp
```

### C API

```c
#include <gnss/gnsslib.h>

GNSSGeneratorHandle* gen = gnss_create_generator();
if (gnss_init_from_json(gen, "config.json")) {
    gnss_generate(gen, 10.0);       // 10 másodperc
    gnss_save(gen, "output.bin");

    double duration, sizeMB;
    gnss_get_stats(gen, &duration, &sizeMB);
}
gnss_destroy_generator(gen);
```

### C++ API

```cpp
#include <gnss/gnsslib.h>

gnss::GNSSGenerator generator;
gnss::GNSSConfig config;

config.time.year    = 2021;
config.time.month   = 6;
config.time.day     = 19;
config.position     = { 47.49, 19.01, 120.0 };
config.output.sampleRate = 4.0e6;
config.ephemerisFile     = "BRDC00WRD_R_20211700000_01D_GN.rnx";

if (generator.initialize(config)) {
    generator.generate(10.0);
    generator.saveToFile("output.bin");
}
```

## Telepítés

Linux alatt a `build.sh` alapértelmezésben telepíti:

- `/usr/local/lib/libGNSSLib.so`
- `/usr/local/lib/libGNSSLib.a`
- `/usr/local/include/gnss/*.h`
- `/usr/local/lib/cmake/GNSSLib/*`
- `/usr/local/lib/pkgconfig/gnsslib.pc`

Eltávolítás:

```bash
cd build
sudo xargs rm < install_manifest.txt
```

## Licenc

A projekt a SignalSim / RinexActualizer eredeti licence alatt áll.
Lásd a `LICENSE` fájlt a részletekért.

Copyright (C) 2020–2029 by Jun Mo, All rights reserved.
Kiegészítve és library-vé alakítva.