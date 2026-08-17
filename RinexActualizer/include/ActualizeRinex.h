#ifndef ACTUALIZE_RINEX_H
#define ACTUALIZE_RINEX_H

#include "GnssTime.h"
#include "BasicTypes.h"

// GNSS constellation/system identifiers.
enum class GnssSystem
{
    Gps,
    Bds,
    Galileo,
    Glonass,
    Sbas,
    Qzss,
    NavIC,
    Count
};

void ActualizeRinex(
    UTC_TIME UtcTime,
    CNavData* NavData,
    OUTPUT_PARAM OutputParam
);

void ProcessGpsSystem(
    UTC_TIME UtcTime,
    PGPS_EPHEMERIS Pool,
    int Number
);

void ProcessGlonassSystem(
    UTC_TIME UtcTime,
    PGLONASS_EPHEMERIS Pool,
    int Number
);

#endif