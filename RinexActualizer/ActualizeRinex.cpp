
//----------------------------------------------------------------------
// ActualizeRinex.cpp:
//   Actualize RINEX ephemeris data to simulation start time
//
//   IMPORTANT:
//
//   The parser stores GPS_EPHEMERIS.week in the following way:
//
//       GPS      -> GPS week
//       BDS      -> BDS week
//       Galileo  -> GPS week
//
//   See DecodeEphParam():
//
//       GPS:      Eph->week = GPS week
//       BDS:      Eph->week = GPS week - 1356
//       Galileo:  Eph->week = Galileo week - 1024 = GPS week
//
//   Therefore the internal absolute-time representation used here is:
//
//       GPS:      week + 0
//       BDS:      week + 1356
//       Galileo:  week + 0
//
//   The simulation time and RINEX time are converted using the SAME
//   UTC -> GNSS time convention (currently leap seconds disabled,
//   because the RINEX parser uses UtcToGpsTime(..., FALSE)).
//
//   One constant offset is calculated from the EARLIEST RINEX TOC:
//
//       Offset = SimulationStart - EarliestRinexTOC
//
//   The same offset is applied to TOC and TOE of every ephemeris.
//
//----------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "ConstVal.h"
#include "BasicTypes.h"
#include "GnssTime.h"
#include "NavData.h"
#include "ActualizeRinex.h"


//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------

static const int64_t GPS_WEEK_SECONDS = 604800;


//----------------------------------------------------------------------
// Convert constellation-specific week representation to an absolute
// GPS-epoch based second count.
//
// IMPORTANT:
//
// GPS_EPHEMERIS.week representation is defined by DecodeEphParam():
//
//   GPS:
//       Eph->week = GPS week
//
//   BDS:
//       Eph->week = GPS week - 1356
//
//   Galileo:
//       Eph->week = Galileo week - 1024
//                  = GPS week
//
// Therefore:
//
//   GPS      -> +0
//   BDS      -> +1356
//   Galileo  -> +0
//
// DO NOT use +1024 for Galileo here.
//----------------------------------------------------------------------

static int64_t GetWeekEpochOffset(GnssSystem System)
{
    switch (System)
    {
        case BdsSystem:
            return 1356;

        case GalileoSystem:
            return 0;

        case GpsSystem:
        default:
            return 0;
    }
}


//----------------------------------------------------------------------
// Convert system week + TOW to absolute GPS seconds.
//----------------------------------------------------------------------

static int64_t SystemTimeToGpsSeconds(
    GnssSystem System,
    int Week,
    int Seconds)
{
    const int64_t WeekOffset =
        GetWeekEpochOffset(System);

    return ((int64_t)Week + WeekOffset) * GPS_WEEK_SECONDS
         + (int64_t)Seconds;
}


//----------------------------------------------------------------------
// Convert absolute GPS seconds back to the representation used by
// GPS_EPHEMERIS.week.
//
// The result is always normalized:
//
//       0 <= Seconds < 604800
//----------------------------------------------------------------------

static void SplitSystemTime(
    GnssSystem System,
    int64_t AbsoluteSeconds,
    int *Week,
    int *Seconds)
{
    const int64_t WeekOffset =
        GetWeekEpochOffset(System);

    int64_t AbsoluteWeek =
        AbsoluteSeconds / GPS_WEEK_SECONDS;

    int64_t Tow =
        AbsoluteSeconds % GPS_WEEK_SECONDS;

    // C/C++ integer division truncates toward zero.
    // Correct negative values explicitly.
    if (Tow < 0)
    {
        Tow += GPS_WEEK_SECONDS;
        AbsoluteWeek--;
    }

    int64_t NativeWeek =
        AbsoluteWeek - WeekOffset;

    *Week = (int)NativeWeek;
    *Seconds = (int)Tow;
}


//----------------------------------------------------------------------
// Convert simulation UTC time to absolute GPS-epoch seconds.
//
// IMPORTANT:
//
// The RINEX parser uses:
//
//       UtcToGpsTime(time, FALSE)
//
// Therefore the simulation time must use exactly the same convention.
//
// Do NOT change this to TRUE unless the complete signal-generation
// chain also uses leap-second corrected GPS time.
//----------------------------------------------------------------------

static int64_t UtcToAbsoluteGpsSeconds(
    UTC_TIME UtcTime)
{
    GNSS_TIME GpsTime =
        UtcToGpsTime(UtcTime, FALSE);

    return (int64_t)GpsTime.Week * GPS_WEEK_SECONDS
         + (int64_t)(GpsTime.MilliSeconds / 1000);
}


//----------------------------------------------------------------------
// Get simulation start time in the week representation used by
// GPS_EPHEMERIS.
//
// GPS:
//       GPS week
//
// BDS:
//       BDS week
//
// Galileo:
//       GPS week
//
// The result is primarily useful for diagnostics.
//----------------------------------------------------------------------

static void GetSimulationSystemTime(
    UTC_TIME UtcTime,
    GnssSystem System,
    int *Week,
    int *Seconds)
{
    GNSS_TIME Time;

    switch (System)
    {
        case BdsSystem:
            Time = UtcToBdsTime(UtcTime);
            break;

        case GalileoSystem:
        case GpsSystem:
        default:
            Time = UtcToGpsTime(UtcTime, FALSE);
            break;
    }

    *Week =
        Time.Week;

    *Seconds =
        Time.MilliSeconds / 1000;
}


//----------------------------------------------------------------------
// Find the earliest valid RINEX TOC in the ephemeris pool.
//
// DO NOT assume Pool[0] is the first RINEX epoch.
//
// The ephemeris pool may contain multiple satellites and multiple
// epochs, and therefore the first array element is not necessarily
// the earliest time.
//----------------------------------------------------------------------

static int FindEarliestEphemeris(
    GnssSystem System,
    PGPS_EPHEMERIS Pool,
    int EphemerisNumber)
{
    if (Pool == 0 || EphemerisNumber <= 0)
        return -1;

    int EarliestIndex = -1;

    int64_t EarliestTime = INT64_MAX;

    for (int i = 0; i < EphemerisNumber; i++)
    {
        const PGPS_EPHEMERIS Eph =
            &Pool[i];

        if (Eph->valid == 0)
            continue;

        const int64_t TocTime =
            SystemTimeToGpsSeconds(
                System,
                Eph->week,
                Eph->toc);

        if (EarliestIndex < 0 ||
            TocTime < EarliestTime)
        {
            EarliestIndex = i;
            EarliestTime = TocTime;
        }
    }

    return EarliestIndex;
}


//----------------------------------------------------------------------
// Recalculate derived ephemeris values.
//
// The actualization changes TOE and therefore omega_t must be rebuilt.
//
// The following quantities do NOT need recalculation because they
// depend only on the orbital parameters:
//
//       axis
//       n
//       root_ecc
//       omega_delta
//
//----------------------------------------------------------------------

static void RecalculateDerivedTimeFields(
    GnssSystem System,
    PGPS_EPHEMERIS Eph)
{
    if (Eph == 0)
        return;

    if (System == BdsSystem)
    {
        Eph->omega_t =
            Eph->omega0
            - CGCS2000_OMEGDOTE * Eph->toe;
    }
    else
    {
        Eph->omega_t =
            Eph->omega0
            - WGS_OMEGDOTE * Eph->toe;
    }
}


//----------------------------------------------------------------------
// Actualize one GNSS constellation.
//
// The operation is:
//
//       Offset = SimulationStart - EarliestRinexTOC
//
// For every ephemeris:
//
//       NewTOC = OldTOC + Offset
//       NewTOE = OldTOE + Offset
//
// The absolute representation is used during the calculation so
// week-boundary crossings are handled correctly.
//
//----------------------------------------------------------------------

void ProcessGnssSystem(
    UTC_TIME UtcTime,
    GnssSystem System,
    PGPS_EPHEMERIS Pool,
    int EphemerisNumber)
{
    if (Pool == 0 || EphemerisNumber <= 0)
        return;


    //------------------------------------------------------------------
    // Simulation start in absolute GPS seconds.
    //------------------------------------------------------------------

    const int64_t SimTotalSec =
        UtcToAbsoluteGpsSeconds(UtcTime);


    //------------------------------------------------------------------
    // Find the EARLIEST valid RINEX ephemeris.
    //------------------------------------------------------------------

    const int FirstIndex =
        FindEarliestEphemeris(
            System,
            Pool,
            EphemerisNumber);

    if (FirstIndex < 0)
        return;


    PGPS_EPHEMERIS First =
        &Pool[FirstIndex];


    //------------------------------------------------------------------
    // First RINEX epoch.
    //
    // DecodeEphParam() stores the RINEX epoch in Eph->toc.
    //------------------------------------------------------------------

    const int64_t FirstTotalSec =
        SystemTimeToGpsSeconds(
            System,
            First->week,
            First->toc);


    //------------------------------------------------------------------
    // One constant time offset for the complete constellation.
    //------------------------------------------------------------------

    const int64_t OffsetSec =
        SimTotalSec - FirstTotalSec;


    //------------------------------------------------------------------
    // Apply the offset to every valid ephemeris.
    //------------------------------------------------------------------

    for (int i = 0; i < EphemerisNumber; i++)
    {
        PGPS_EPHEMERIS Eph =
            &Pool[i];

        if (Eph->valid == 0)
            continue;


        //----------------------------------------------------------------
        // Original absolute TOC.
        //----------------------------------------------------------------

        const int64_t OriginalTocTotal =
            SystemTimeToGpsSeconds(
                System,
                Eph->week,
                Eph->toc);


        //----------------------------------------------------------------
        // Original absolute TOE.
        //
        // TOE belongs to Eph->week in the current structure.
        //----------------------------------------------------------------

        const int64_t OriginalToeTotal =
            SystemTimeToGpsSeconds(
                System,
                Eph->week,
                Eph->toe);


        //----------------------------------------------------------------
        // Shift both times by exactly the same offset.
        //----------------------------------------------------------------

        const int64_t NewTocTotal =
            OriginalTocTotal + OffsetSec;

        const int64_t NewToeTotal =
            OriginalToeTotal + OffsetSec;


        //----------------------------------------------------------------
        // Convert shifted TOC back to constellation representation.
        //----------------------------------------------------------------

        int NewTocWeek = 0;
        int NewToc = 0;

        SplitSystemTime(
            System,
            NewTocTotal,
            &NewTocWeek,
            &NewToc);


        //----------------------------------------------------------------
        // Convert shifted TOE back to constellation representation.
        //----------------------------------------------------------------

        int NewToeWeek = 0;
        int NewToe = 0;

        SplitSystemTime(
            System,
            NewToeTotal,
            &NewToeWeek,
            &NewToe);


        //----------------------------------------------------------------
        // IMPORTANT:
        //
        // GPS_EPHEMERIS contains only one week field.
        //
        // The ephemeris selection logic uses:
        //
        //       Eph->week
        //       Eph->toe
        //
        // Therefore Eph->week must be the week belonging to TOE.
        //----------------------------------------------------------------

        Eph->week =
            NewToeWeek;

        Eph->toe =
            NewToe;

        Eph->toc =
            NewToc;


        //----------------------------------------------------------------
        // DecodeEphParam() currently defines:
        //
        //       Eph->top = Eph->toe;
        //
        // Keep it synchronized.
        //----------------------------------------------------------------

        Eph->top =
            Eph->toe;


        //----------------------------------------------------------------
        // Recalculate time-dependent derived value.
        //----------------------------------------------------------------

        RecalculateDerivedTimeFields(
            System,
            Eph);
    }
}


//----------------------------------------------------------------------
// GLONASS
//
// GLONASS is intentionally not actualized here.
//
// The current GLONASS_TIME representation does not provide a complete
// absolute four-year cycle representation, therefore shifting GLONASS
// ephemerides safely requires additional time information.
//
// Do not fake this conversion.
//----------------------------------------------------------------------

void ProcessGlonassSystem(
    UTC_TIME UtcTime,
    PGLONASS_EPHEMERIS Pool,
    int EphemerisNumber)
{
    (void)UtcTime;
    (void)Pool;
    (void)EphemerisNumber;
}


//----------------------------------------------------------------------
// Public entry point.
//----------------------------------------------------------------------

void ActualizeRinex(
    UTC_TIME UtcTime,
    CNavData *NavData,
    const OUTPUT_PARAM& OutputParam)
{
    if (NavData == 0)
        return;


    //------------------------------------------------------------------
    // GPS
    //------------------------------------------------------------------

    if (OutputParam.FreqSelect[GpsSystem] != 0)
    {
        ProcessGnssSystem(
            UtcTime,
            GpsSystem,
            NavData->GetEphemerisPool(GpsSystem),
            NavData->GetEphemerisNumber(GpsSystem));
    }


    //------------------------------------------------------------------
    // BDS
    //------------------------------------------------------------------

    if (OutputParam.FreqSelect[BdsSystem] != 0)
    {
        ProcessGnssSystem(
            UtcTime,
            BdsSystem,
            NavData->GetEphemerisPool(BdsSystem),
            NavData->GetEphemerisNumber(BdsSystem));
    }


    //------------------------------------------------------------------
    // Galileo
    //------------------------------------------------------------------

    if (OutputParam.FreqSelect[GalileoSystem] != 0)
    {
        ProcessGnssSystem(
            UtcTime,
            GalileoSystem,
            NavData->GetEphemerisPool(GalileoSystem),
            NavData->GetEphemerisNumber(GalileoSystem));
    }


    //------------------------------------------------------------------
    // GLONASS
    //------------------------------------------------------------------

    if (OutputParam.FreqSelect[GlonassSystem] != 0)
    {
        ProcessGlonassSystem(
            UtcTime,
            NavData->GetGlonassEphemerisPool(),
            NavData->GetGlonassEphemerisNumber());
    }
}

