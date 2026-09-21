# GNSSLib

## Windows build

A SW stack buildelését a `GNSSLib/build.bat` script teszi lehetővé.

A script alapértelmezés szerint **Release** konfigurációban fordít, és a build előtt törli az előző `build` könyvtárat.

```bat
.\build.bat
```

Sikeres build esetén a lefordított binárisok a `build/bin` könyvtárba kerülnek:

```text
build/
└── bin/
    ├── IFdataGen.exe
    ├── RinexActualizerApp.exe
    ├── stream_test.exe
    ├── stream_usage.exe
    └── ...
```

A script ezt követően lefuttatja a CMake install lépést is. Az installált fájlok az `out/install` könyvtárba kerülnek:

```text
out/
└── install/
    ├── bin/
    │   ├── IFdataGen.exe
    │   └── RinexActualizerApp.exe
    ├── include/
    └── lib/
```

A `build/bin` könyvtár a fejlesztés és tesztelés során használható közvetlenül, míg az `out/install` könyvtár a CMake által definiált telepített csomagot tartalmazza.

### Build konfiguráció

A build alapértelmezett beállításai:

```text
BUILD_TYPE=Release
BUILD_DIR=build
INSTALL_PREFIX=out/install
USE_NATIVE_OPT=OFF
```

A konfigurációk környezeti változókkal felülírhatók. Például Debug build:

```powershell
$env:BUILD_TYPE="Debug"
.\build.bat
```
### Használat

A sikeres build után az alkalmazások a `build/bin` könyvtárból indíthatók.

#### IFdataGen

Az `IFdataGen` konfigurációs JSON fájl alapján generál IF adatot:

```powershell
.\build\bin\IFdataGen.exe .\SignalSim\IFdataGen\configs\GPS_L1_0616.json
```

#### RinexActualizerApp

A RINEX fájlok aktualizálására szolgáló alkalmazás:

```powershell
.\build\bin\RinexActualizerApp.exe
```

#### Stream test

A `stream_test` az IF adatfolyamot blokkokban olvassa, majd az egyes komplex mintákat (`std::complex<float>`) egyenként dolgozza fel:

```powershell
.\build\bin\stream_test.exe .\SignalSim\IFdataGen\configs\GPS_L1_0616.json
```

A bemeneti adatfolyam egy eleme:

```text
std::complex<float>
```

amely két `float` értéket tartalmaz:

```text
I + jQ
```

Ennek mérete:

```text
sizeof(std::complex<float>) = 8 byte
```

A `stream_test` használható annak ellenőrzésére, hogy az IF adatgenerálás és a GNSSLib streaming feldolgozási lánca megfelelően működik.

#### Stream usage

A `stream_usage` példa az `IfGenEngine::Stream()` API használatát mutatja:

```powershell
.\build\bin\stream_usage.exe
```

A stream API callback alapú feldolgozást biztosít. A callback egyszerre több mintát kap, amelyek szükség esetén egyenként feldolgozhatók:

```cpp
for (std::size_t i = 0; i < n; ++i) {
    const std::complex<float>& sample = samples[i];

    // Egyetlen I/Q minta feldolgozása
}
```

### Telepített verzió

A `cmake --install` lépés után az installált alkalmazások az `out/install/bin` könyvtárban találhatók:

```text
out/
└── install/
    └── bin/
        ├── IFdataGen.exe
        └── RinexActualizerApp.exe
```

Fejlesztés és tesztelés során a `build/bin` könyvtár használata javasolt.

## Linux / macOS build

A SW stack buildelését a `GNSSLib/build.sh` script teszi lehetővé.

A script alapértelmezés szerint **Release** konfigurációban fordít, a rendelkezésre álló CPU magok számának megfelelő párhuzamosítással, majd sikeres build esetén telepíti a GNSSLib-et a rendszerbe.

### Előfeltételek

A buildhez az alábbi eszközök szükségesek:

* CMake
* C++ compiler
* Ninja vagy Make alapú CMake generator
* Git, ha a projekt forráskódját Git repositoryból használjuk

Ubuntu/Debian rendszeren például:

```bash
sudo apt update
sudo apt install build-essential cmake
```

A telepítés ellenőrzése:

```bash
cmake --version
g++ --version
```

### Build

A `GNSSLib` könyvtárból:

```bash
./build.sh
```

Ha a script még nem futtatható, először végrehajtási jogosultságot kell adni:

```bash
chmod +x build.sh
```

majd:

```bash
./build.sh
```

A script a build előtt törli az előző `build` könyvtárat, ezért minden futtatás **clean buildet** eredményez.

Sikeres build esetén a lefordított binárisok a `build/bin` könyvtárba kerülnek:

```text
build/
└── bin/
    ├── IFdataGen
    ├── RinexActualizerApp
    ├── stream_test
    ├── stream_usage
    ├── basic_usage
    └── ...
```

A könyvtárban találhatók továbbá a GNSSLib fordított library fájljai:

```text
build/
└── lib/
    ├── ...
```

### Build konfiguráció

A `build.sh` az alábbi alapértelmezett beállításokat használja:

```text
BUILD_TYPE=Release
BUILD_DIR=build
INSTALL_PREFIX=/usr/local
DO_INSTALL=1
DO_TEST=1
USE_NATIVE_OPT=ON
```

A build beállításai környezeti változókkal felülírhatók.

Például Debug build:

```bash
BUILD_TYPE=Debug ./build.sh
```

Másik build könyvtár használata:

```bash
BUILD_DIR=build-debug BUILD_TYPE=Debug ./build.sh
```

A natív CPU optimalizáció kikapcsolása:

```bash
USE_NATIVE_OPT=OFF ./build.sh
```

Az automatikus install kikapcsolása:

```bash
DO_INSTALL=0 ./build.sh
```

A `basic_usage` automatikus futtatásának kikapcsolása:

```bash
DO_TEST=0 ./build.sh
```

A beállítások kombinálhatók:

```bash
BUILD_TYPE=Release \
USE_NATIVE_OPT=ON \
DO_TEST=0 \
DO_INSTALL=0 \
./build.sh
```

### Telepítés

A script sikeres build után alapértelmezés szerint lefuttatja:

```bash
cmake --install build
```

Az alapértelmezett install prefix:

```text
/usr/local
```

Ha a telepítési könyvtár nem írható a felhasználó számára, a script automatikusan `sudo` segítségével próbálja végrehajtani az install lépést.

Az install prefix megváltoztatható:

```bash
INSTALL_PREFIX=/opt/gnsslib ./build.sh
```

Ebben az esetben a telepített fájlok az `/opt/gnsslib` könyvtár alá kerülnek.

### Használat

A sikeres build után az alkalmazások közvetlenül a `build/bin` könyvtárból futtathatók.

#### IFdataGen

Az `IFdataGen` konfigurációs JSON fájl alapján generál IF adatot:

```bash
./build/bin/IFdataGen ./SignalSim/IFdataGen/configs/GPS_L1_0616.json
```

#### RinexActualizerApp

A RINEX fájlok aktualizálására szolgáló alkalmazás:

```bash
./build/bin/RinexActualizerApp
```

#### Stream test

A `stream_test` az IF adatfolyam feldolgozását teszteli. A komplex mintákat (`std::complex<float>`) a stream callbacken keresztül kapja meg:

```bash
./build/bin/stream_test ./SignalSim/IFdataGen/configs/GPS_L1_0616.json
```

Egy stream elem típusa:

```text
std::complex<float>
```

amely egy I/Q mintát reprezentál:

```text
I + jQ
```

A `std::complex<float>` mérete:

```text
8 byte
```

A stream callback egyszerre több mintát kap, amelyek szükség esetén egyenként dolgozhatók fel:

```cpp
for (std::size_t i = 0; i < n; ++i) {
    const std::complex<float>& sample = samples[i];

    // Egyetlen I/Q minta feldolgozása
}
```

A `stream_test` használható annak ellenőrzésére, hogy az IF adatgenerálás és a GNSSLib streaming feldolgozási lánca megfelelően működik.

#### Stream usage

A `stream_usage` példa az `IfGenEngine::Stream()` API használatát mutatja:

```bash
./build/bin/stream_usage
```

### Telepített verzió használata

A `cmake --install` után a telepített fájlok az `INSTALL_PREFIX` alatt találhatók.

Az alapértelmezett telepítési hely:

```text
/usr/local
```

A telepített struktúra például:

```text
/usr/local/
├── bin/
│   ├── IFdataGen
│   └── RinexActualizerApp
├── include/
│   └── ...
└── lib/
    ├── ...
    └── ...
```

A `build/bin` könyvtár a fejlesztés és tesztelés során használható közvetlenül, míg az `/usr/local` alatt található verzió a rendszerbe telepített GNSSLib példány.

### GNSSLib használata saját CMake projektben

A telepített GNSSLib saját CMake projektből `find_package()` segítségével használható:

```cmake
find_package(GNSSLib REQUIRED)

target_link_libraries(myapp PRIVATE GNSSLib::GNSSLib)
```

Ha a telepítés nem standard helyre történt, a CMake számára megadható az install prefix:

```bash
cmake -S . -B build \
    -DCMAKE_PREFIX_PATH=/opt/gnsslib
```

### pkg-config

A GNSSLib `pkg-config` segítségével is használható, amennyiben a telepítés során a megfelelő `.pc` fájl rendelkezésre áll:

```bash
pkg-config --cflags --libs gnsslib
```

Saját build rendszerben például:

```bash
g++ main.cpp $(pkg-config --cflags --libs gnsslib) -o myapp
```

### Clean rebuild

A `build.sh` alapértelmezés szerint minden futtatás elején törli a build könyvtárat:

```bash
rm -rf "${BUILD_DIR}"
```

Ezért a következő parancs mindig tiszta buildet eredményez:

```bash
./build.sh
```

Ha csak a meglévő buildet szeretnénk újrafordítani, a `build.sh` használata helyett közvetlenül a CMake build parancs futtatható:

```bash
cmake --build build --parallel
```


