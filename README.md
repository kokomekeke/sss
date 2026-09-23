# IFDataGen – GNSS IF adatgenerátor

Nagy teljesítményű GNSS közbenső frekvenciás (IF) jelgenerátor
GPS, Galileo, BDS és GLONASS rendszerekhez.

A projekt a [globsky/SignalSim](https://github.com/globsky/SignalSim) motorjára épül,
de **annak forrásfájljait nem módosítja**: a `SignalSim/` és a
`RinexActualizer/` könyvtárak érintetlenek maradnak. Minden
kiegészítés – a strukturált `IfGenParams` setup-út, a `GNSSConfig`
public API, a `GNSSGenerator` magas szintű burkoló, a C API – a
`Sisi/GNSSLib/` alatt él.

---

## Funkciók

- **Több csillagkép**: GPS, Galileo, BDS, GLONASS
- **Több jel**:
  - GPS: L1CA, L1C, L2C, L2P, L5
  - Galileo: E1, E5a, E5b, E5, E6
  - BDS: B1C, B1I, B2I, B3I, B2a, B2b
  - GLONASS: G1, G2
    (A G3 – CDMA L3 – deklarálva van a public API-ban, de a jelenlegi
     SignalSim build nem garantáltan tartalmazza a hozzá tartozó
     `SIGNAL_INDEX_G3` / `FREQ_GLO_G3` szimbólumokat, ezért a
     `ValidateIfGenParams()` elutasítja. Lásd lentebb a *Ismert
     korlátok* szakaszt.)
- **RINEX 3.x / 4.x** navigációs fájl támogatás
- **Dinamikus pálya**: állandó sebesség, gyorsulás, vízszintes kanyar;
  a `VerticalAcc` és `Jerk` szegmenstípusok elfogadottak, de a
  jelenlegi SignalSim buildben `ConstAcc`-re esnek vissza (lásd
  *Ismert korlátok*)
- **Valósághű modellezés**: Doppler, ionoszférikus / troposzférikus késleltetés
- **Kimeneti formátumok**: IQ2, IQ4, IQ8, IQ16
- **OpenMP párhuzamosítás**
- **JSON konfiguráció** (visszafelé kompatibilis)
- **Strukturált C++ API** (`GNSSConfig` → `IfGenParams` → `IfGenEngine`)
- **Streaming API** (`read()`, `stream()`) fájlírás nélkül is
- **C API** a C-ből hívó alkalmazásokhoz

---

## Követelmények

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ gcc make cmake libomp-dev