#ifndef ACTUALIZE_RINEX_H
#define ACTUALIZE_RINEX_H

#include "GnssTime.h"
#include "BasicTypes.h"

typedef enum {
    GpsSystem,
    BdsSystem,
    GalileoSystem,
    GlonassSystem,
    SbasSystem,
    QzssSystem,
    NavICSystem
} GnssSystem;

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

#endif