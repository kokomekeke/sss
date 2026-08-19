#ifndef ACTUALIZE_RINEX_H
#define ACTUALIZE_RINEX_H

#include "BasicTypes.h"   // GnssSystem, UTC_TIME, OUTPUT_PARAM, PGPS_EPHEMERIS, PGLONASS_EPHEMERIS
#include "GnssTime.h"     // UtcToGpsTime(), UtcToGlonassTime()

// FONTOS: a GnssSystem enum már definiálva van a BasicTypes.h-ban.
// Ne definiáld újra itt - ütköző deklarációs hibát adna fordításkor.

class CNavData;   // forward declaration elég - csak pointerként használjuk itt,
                   // a teljes definíció csak ActualizeRinex.cpp-ben kell

// Fő belépési pont: az összes engedélyezett (OutputParam.FreqSelect szerint)
// konstelláció efemerisz adatait a NavData poolokban a UtcTime időpontra
// aktualizálja, a már betöltött adatokon, in-place.
void ActualizeRinex(
    UTC_TIME UtcTime,
    CNavData* NavData,
    const OUTPUT_PARAM& OutputParam
);

// GPS / BDS / Galileo aktualizálása. A System paraméter azért kell,
// mert a poolban tárolt "week" mező rendszerenként MÁS epocha szerint
// van kódolva (lásd Rinex.cpp DecodeEphParam):
//   GPS     -> nyers GPS-hét
//   BDS     -> GPS-hét - 1356
//   Galileo -> GPS-hét - 1024
void ProcessGnssSystem(
    UTC_TIME UtcTime,
    GnssSystem System,
    PGPS_EPHEMERIS Pool,
    int Number
);

// GLONASS aktualizálása - külön függvény, mert más időstruktúrát
// (day + tk + tb) használ, nem hét+másodperc formátumot.
void ProcessGlonassSystem(
    UTC_TIME UtcTime,
    PGLONASS_EPHEMERIS Pool,
    int Number
);

#endif