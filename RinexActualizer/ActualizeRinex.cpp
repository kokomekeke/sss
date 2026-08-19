#include "ActualizeRinex.h"
#include "NavData.h"
#include <cstdint>
#include <climits>


//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------

static constexpr long long GPS_WEEK_SECONDS = 604800LL;

static constexpr long long GLO_DAY_SECONDS = 86400LL;
static constexpr long long GLO_CYCLE_DAYS = 1461LL;
static constexpr long long GLO_CYCLE_SECONDS =
    GLO_CYCLE_DAYS * GLO_DAY_SECONDS;

static constexpr int GLO_FRAME_SECONDS = 30;
static constexpr int GLO_FRAMES_PER_DAY = 2880;
static constexpr int GLO_TB_SECONDS = 900;


//----------------------------------------------------------------------
// GNSS week epoch handling
//----------------------------------------------------------------------
//
// Internal absolute time is always represented in GPS epoch.
//
// GPS:
//     system week = GPS week
//
// BDS:
//     system week = GPS week - 1356
//
// Galileo:
//     system week = GPS week - 1024
//
// Therefore:
//
//     GPS week = system week + offset
//
//----------------------------------------------------------------------

static inline int GetWeekEpochOffset(GnssSystem System)
{
    switch (System)
    {
        case BdsSystem:
            return 1356;

        case GalileoSystem:
            return 1024;

        case GpsSystem:
        default:
            return 0;
    }
}


//----------------------------------------------------------------------
// Convert system-specific week + SOW to absolute GPS seconds.
//----------------------------------------------------------------------

static inline long long SystemTimeToGpsSeconds(
    GnssSystem System,
    int Week,
    int Seconds)
{
    const long long GpsWeek =
        static_cast<long long>(Week) +
        GetWeekEpochOffset(System);

    return
        GpsWeek * GPS_WEEK_SECONDS +
        static_cast<long long>(Seconds);
}


//----------------------------------------------------------------------
// Split absolute GPS seconds into system-specific week + SOW.
//
// The input is always GPS-epoch based.
// The output week is converted back to the selected
// constellation's native epoch.
//----------------------------------------------------------------------

static inline void SplitSystemTime(
    GnssSystem System,
    long long TotalSec,
    int& Week,
    int& Sec)
{
    long long GpsWeek;

    if (TotalSec >= 0)
    {
        GpsWeek =
            TotalSec / GPS_WEEK_SECONDS;
    }
    else
    {
        GpsWeek =
            (TotalSec - (GPS_WEEK_SECONDS - 1)) /
            GPS_WEEK_SECONDS;
    }

    Sec = static_cast<int>(
        TotalSec -
        GpsWeek * GPS_WEEK_SECONDS
    );

    Week = static_cast<int>(
        GpsWeek -
        GetWeekEpochOffset(System)
    );
}


//----------------------------------------------------------------------
// GLONASS tk
//----------------------------------------------------------------------
//
// SignalSim encoding:
//
//     tk = ((FrameTime / 120) << 7) +
//          (FrameTime % 120)
//
// FrameTime is:
//
//     0 ... 2879
//
// One FrameTime unit:
//
//     30 seconds
//
// Therefore:
//
//     0      -> 00:00:00
//     1      -> 00:00:30
//     2      -> 00:01:00
//     ...
//----------------------------------------------------------------------

static inline int DecodeGlonassFrameTime(
    unsigned short tk)
{
    const int Hour =
        static_cast<int>(tk >> 7);

    const int Frame =
        static_cast<int>(tk & 0x7F);

    return
        Hour * 120 +
        Frame;
}


//----------------------------------------------------------------------
// Convert SignalSim tk to seconds within day.
//----------------------------------------------------------------------

static inline long long GlonassTkToSeconds(
    unsigned short tk)
{
    return
        static_cast<long long>(
            DecodeGlonassFrameTime(tk)
        ) *
        GLO_FRAME_SECONDS;
}


//----------------------------------------------------------------------
// Encode 30-second frame time into SignalSim tk.
//----------------------------------------------------------------------

static inline unsigned short EncodeGlonassTk(
    int FrameTime)
{
    FrameTime %= GLO_FRAMES_PER_DAY;

    if (FrameTime < 0)
        FrameTime += GLO_FRAMES_PER_DAY;

    return static_cast<unsigned short>(
        ((FrameTime / 120) << 7) +
        (FrameTime % 120)
    );
}


//----------------------------------------------------------------------
// Normalize GLONASS day + seconds.
//
// Day is 1-based:
//
//     1 ... 1461
//
// Seconds:
//
//     0 ... 86399
//
// The function handles crossing both ends of the 4-year cycle.
//----------------------------------------------------------------------

static inline void NormalizeGlonassDayTime(
    long long& Day,
    long long& Seconds)
{
    while (Seconds < 0)
    {
        Seconds += GLO_DAY_SECONDS;
        --Day;
    }

    while (Seconds >= GLO_DAY_SECONDS)
    {
        Seconds -= GLO_DAY_SECONDS;
        ++Day;
    }

    /*
     * Keep the day in the 1...1461 range.
     */
    while (Day < 1)
        Day += GLO_CYCLE_DAYS;

    while (Day > GLO_CYCLE_DAYS)
        Day -= GLO_CYCLE_DAYS;
}


//----------------------------------------------------------------------
// Split cycle-relative GLONASS seconds into:
//
//     1-based day
//     30-second FrameTime
//
// Input:
//
//     0 ... 1461*86400
//----------------------------------------------------------------------

static inline void SplitGlonassTime(
    long long TotalSec,
    int& Day,
    int& FrameTime)
{
    /*
     * Normalize the input to one 4-year cycle.
     */
    TotalSec %= GLO_CYCLE_SECONDS;

    if (TotalSec < 0)
        TotalSec += GLO_CYCLE_SECONDS;


    /*
     * 0-based day.
     */
    const long long DayNumber =
        TotalSec / GLO_DAY_SECONDS;


    /*
     * Seconds inside the day.
     */
    const long long SecondsOfDay =
        TotalSec % GLO_DAY_SECONDS;


    /*
     * SignalSim uses 1-based day.
     */
    Day =
        static_cast<int>(DayNumber + 1);


    /*
     * Round to nearest 30-second frame.
     */
    FrameTime =
        static_cast<int>(
            (SecondsOfDay + GLO_FRAME_SECONDS / 2) /
            GLO_FRAME_SECONDS
        );


    /*
     * 23:59:45 ... 23:59:59 rounds to next day.
     */
    if (FrameTime >= GLO_FRAMES_PER_DAY)
    {
        FrameTime = 0;
        ++Day;

        if (Day > GLO_CYCLE_DAYS)
            Day = 1;
    }
}


//----------------------------------------------------------------------
// Convert GLONASS_TIME to absolute seconds.
//
// This is SignalSim's internal GLONASS representation.
//
// LeapYear:
//     4-year cycle number
//
// Day:
//     1-based day within cycle
//
// MilliSeconds:
//     internal time-of-day
//
// Note:
//
// UtcToGlonassTime() adds +3 hours to MilliSeconds.
// We intentionally use that same representation here so
// RINEX-derived ephemeris and simulation time use the same
// time coordinate.
//----------------------------------------------------------------------

static inline long long GlonassTimeToAbsoluteSeconds(
    const GLONASS_TIME& Time)
{
    return
        static_cast<long long>(Time.LeapYear) *
        GLO_CYCLE_SECONDS
        +
        static_cast<long long>(Time.Day - 1) *
        GLO_DAY_SECONDS
        +
        static_cast<long long>(Time.MilliSeconds) /
        1000LL;
}


//----------------------------------------------------------------------
// Return the signed difference between two times-of-day,
// wrapped to approximately +/-12 hours.
//
// This is used to determine on which day tk belongs relative
// to tb.
//
// Example:
//
//     tb = 23:59:00
//     tk = 00:01:00
//
// naive difference:
//
//     tk - tb = -23:58:00
//
// correct GLONASS day relationship:
//
//     +00:02:00
//----------------------------------------------------------------------

static inline long long GlonassTimeOfDayDifference(
    long long Time1,
    long long Time2)
{
    long long Difference =
        Time1 - Time2;

    while (Difference <= -GLO_DAY_SECONDS / 2)
        Difference += GLO_DAY_SECONDS;

    while (Difference > GLO_DAY_SECONDS / 2)
        Difference -= GLO_DAY_SECONDS;

    return Difference;
}


//----------------------------------------------------------------------
// Determine the day of tk from:
//
//     tb day
//     tb seconds
//     tk seconds
//
// tk itself contains no day information.
//
// The closest day to tb is selected.
//----------------------------------------------------------------------

static inline long long GetTkAbsoluteDay(
    long long TbDay,
    long long TbSeconds,
    long long TkSeconds)
{
    const long long Difference =
        GlonassTimeOfDayDifference(
            TkSeconds,
            TbSeconds
        );

    /*
     * If tk is e.g. 00:05 and tb is 23:55,
     * tk belongs to the following day.
     */
    if (Difference < 0)
        return TbDay - 1;

    if (TkSeconds < TbSeconds)
        return TbDay + 1;

    return TbDay;
}


//----------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------

void ActualizeRinex(
    UTC_TIME UtcTime,
    CNavData* NavData,
    OUTPUT_PARAM OutputParam)
{
    if (NavData == nullptr)
        return;


    PGPS_EPHEMERIS Pool = nullptr;
    PGLONASS_EPHEMERIS GlonassPool = nullptr;

    int Number = 0;


    for (int i = GpsSystem;
         i <= GlonassSystem;
         ++i)
    {
        const GnssSystem System =
            static_cast<GnssSystem>(i);


        if (!OutputParam.FreqSelect[System])
            continue;


        switch (System)
        {
            case GpsSystem:
            case BdsSystem:
            case GalileoSystem:
            {
                Pool =
                    NavData->GetEphemerisPool(System);

                Number =
                    NavData->GetEphemerisNumber(System);


                ProcessGnssSystem(
                    UtcTime,
                    System,
                    Pool,
                    Number
                );

                break;
            }


            case GlonassSystem:
            {
                GlonassPool =
                    NavData->GetGlonassEphemerisPool();

                Number =
                    NavData->GetGlonassEphemerisNumber();


                ProcessGlonassSystem(
                    UtcTime,
                    GlonassPool,
                    Number
                );

                break;
            }


            case SbasSystem:
            case QzssSystem:
            case NavICSystem:
            default:
                break;
        }
    }
}


//----------------------------------------------------------------------
// GPS / BDS / Galileo ephemeris actualization
//----------------------------------------------------------------------

void ProcessGnssSystem(
    UTC_TIME UtcTime,
    GnssSystem System,
    PGPS_EPHEMERIS Pool,
    int Number)
{
    if (Pool == nullptr || Number <= 0)
        return;


    //------------------------------------------------------------------
    // Current simulation time.
    //
    // UtcToGpsTime() returns GPS week.
    //
    // FALSE is intentional here because the absolute time coordinate
    // used by the ephemeris pool is the GNSS continuous week coordinate.
    //------------------------------------------------------------------

    const GNSS_TIME SimGnssTime =
        UtcToGpsTime(
            UtcTime,
            FALSE
        );


    const long long SimTotalSec =
        static_cast<long long>(
            SimGnssTime.Week
        ) * GPS_WEEK_SECONDS
        +
        static_cast<long long>(
            SimGnssTime.MilliSeconds / 1000
        );


    //------------------------------------------------------------------
    // First ephemeris is the reference.
    //
    // IMPORTANT:
    //
    // week is interpreted according to System.
    //------------------------------------------------------------------

    PGPS_EPHEMERIS First =
        Pool;


    const long long FirstTotalSec =
        SystemTimeToGpsSeconds(
            System,
            First->week,
            First->toc
        );


    //------------------------------------------------------------------
    // Common time offset.
    //------------------------------------------------------------------

    const long long OffsetSec =
        SimTotalSec - FirstTotalSec;


    //------------------------------------------------------------------
    // Actualize every ephemeris.
    //------------------------------------------------------------------

    for (PGPS_EPHEMERIS Eph = Pool;
         Eph < Pool + Number;
         ++Eph)
    {
        //------------------------------------------------------------------
        // Original TOC.
        //------------------------------------------------------------------

        const long long OrigTocTotal =
            SystemTimeToGpsSeconds(
                System,
                Eph->week,
                Eph->toc
            );


        //------------------------------------------------------------------
        // Original TOE.
        //------------------------------------------------------------------

        const long long OrigToeTotal =
            SystemTimeToGpsSeconds(
                System,
                Eph->week,
                Eph->toe
            );


        //------------------------------------------------------------------
        // Apply offset.
        //------------------------------------------------------------------

        const long long NewTocTotal =
            OrigTocTotal + OffsetSec;

        const long long NewToeTotal =
            OrigToeTotal + OffsetSec;


        //------------------------------------------------------------------
        // Convert TOC back to system-specific week.
        //------------------------------------------------------------------

        int NewWeek;
        int NewToc;

        SplitSystemTime(
            System,
            NewTocTotal,
            NewWeek,
            NewToc
        );


        //------------------------------------------------------------------
        // Convert TOE back to system-specific week.
        //------------------------------------------------------------------

        int NewToeWeek;
        int NewToe;

        SplitSystemTime(
            System,
            NewToeTotal,
            NewToeWeek,
            NewToe
        );


        //------------------------------------------------------------------
        // Store result.
        //------------------------------------------------------------------

        Eph->week = NewWeek;
        Eph->toc = NewToc;
        Eph->toe = NewToe;


        //------------------------------------------------------------------
        // Keep existing SignalSim convention.
        //
        // If top is intended to represent TOE in this structure,
        // this is correct.
        //------------------------------------------------------------------

        Eph->top = NewToe;
    }
}


//----------------------------------------------------------------------
// GLONASS ephemeris actualization
//----------------------------------------------------------------------
//
// IMPORTANT:
//
// GLONASS_EPHEMERIS contains:
//
//     day : 1-based day of leap year / 4-year cycle
//     tb  : seconds within day, 900 s resolution
//     tk  : time of day, 30 s resolution
//
// It does NOT contain LeapYear.
//
// Therefore the RINEX ephemeris is associated with a 4-year
// cycle during actualization.
//
// The first ephemeris is used as the temporal reference.
//
// The actualization offset is:
//
//     simulation_time - first_ephemeris_tb
//
// Then the SAME offset is applied to:
//
//     tb
//     tk
//
// while preserving their relationship.
//----------------------------------------------------------------------

void ProcessGlonassSystem(
    UTC_TIME UtcTime,
    PGLONASS_EPHEMERIS GlonassPool,
    int Number)
{
    if (GlonassPool == nullptr || Number <= 0)
        return;


    //------------------------------------------------------------------
    // Current simulation GLONASS time.
    //------------------------------------------------------------------

    const GLONASS_TIME SimGlo =
        UtcToGlonassTime(
            UtcTime
        );


    //------------------------------------------------------------------
    // Absolute simulation time.
    //
    // LeapYear is the 4-year cycle number.
    // Day is 1-based.
    //------------------------------------------------------------------

    const long long SimTotalSec =
        GlonassTimeToAbsoluteSeconds(
            SimGlo
        );


    //------------------------------------------------------------------
    // First ephemeris is reference.
    //------------------------------------------------------------------

    PGLONASS_EPHEMERIS First =
        GlonassPool;


    //------------------------------------------------------------------
    // First ephemeris tb.
    //
    // tb is already in seconds.
    //
    // IMPORTANT:
    //
    // Do NOT decode tk here.
    // tb is the reference epoch of the ephemeris.
    //------------------------------------------------------------------

    const long long FirstTbSeconds =
        static_cast<long long>(
            First->tb
        );


    //------------------------------------------------------------------
    // First ephemeris day.
    //
    // day is 1-based.
    //------------------------------------------------------------------

    const long long FirstDay =
        static_cast<long long>(
            First->day
        );


    //------------------------------------------------------------------
    // We do not have LeapYear in GLONASS_EPHEMERIS.
    //
    // Associate the first RINEX ephemeris with the simulation
    // cycle that makes its day closest to the current simulation
    // time.
    //------------------------------------------------------------------

    const long long SimCycle =
        static_cast<long long>(
            SimGlo.LeapYear
        );


    const long long SimAbsoluteDay =
        SimCycle * GLO_CYCLE_DAYS
        +
        static_cast<long long>(
            SimGlo.Day - 1
        );


    long long FirstCycle =
        SimCycle;

    long long BestDayDifference =
        LLONG_MAX;


    for (int Delta = -2;
         Delta <= 2;
         ++Delta)
    {
        const long long CandidateCycle =
            SimCycle + Delta;


        const long long CandidateAbsoluteDay =
            CandidateCycle * GLO_CYCLE_DAYS
            +
            (FirstDay - 1);


        const long long Difference =
            CandidateAbsoluteDay -
            SimAbsoluteDay;


        const long long AbsDifference =
            Difference >= 0
                ? Difference
                : -Difference;


        if (AbsDifference < BestDayDifference)
        {
            BestDayDifference =
                AbsDifference;

            FirstCycle =
                CandidateCycle;
        }
    }


    //------------------------------------------------------------------
    // Absolute tb of first ephemeris.
    //------------------------------------------------------------------

    const long long FirstTbTotalSec =
        FirstCycle * GLO_CYCLE_SECONDS
        +
        (FirstDay - 1) * GLO_DAY_SECONDS
        +
        FirstTbSeconds;


    //------------------------------------------------------------------
    // Common actualization offset.
    //------------------------------------------------------------------

    const long long OffsetSec =
        SimTotalSec -
        FirstTbTotalSec;


    //------------------------------------------------------------------
    // Actualize each GLONASS ephemeris.
    //------------------------------------------------------------------

    for (PGLONASS_EPHEMERIS Eph = GlonassPool;
         Eph < GlonassPool + Number;
         ++Eph)
    {
        //------------------------------------------------------------------
        // Original tb.
        //------------------------------------------------------------------

        const long long OrigTbSeconds =
            static_cast<long long>(
                Eph->tb
            );


        //------------------------------------------------------------------
        // Determine cycle/day of this ephemeris.
        //
        // All ephemerides in this RINEX pool are initially interpreted
        // in the same cycle as the first ephemeris.
        //------------------------------------------------------------------

        const long long OrigTbTotalSec =
            FirstCycle * GLO_CYCLE_SECONDS
            +
            static_cast<long long>(
                Eph->day - 1
            ) * GLO_DAY_SECONDS
            +
            OrigTbSeconds;


        //------------------------------------------------------------------
        // New tb absolute time.
        //------------------------------------------------------------------

        const long long NewTbTotalSec =
            OrigTbTotalSec +
            OffsetSec;


        //------------------------------------------------------------------
        // Convert new absolute tb to cycle-relative coordinates.
        //------------------------------------------------------------------

        long long NewCycle =
            NewTbTotalSec /
            GLO_CYCLE_SECONDS;

        long long CycleRelativeSec =
            NewTbTotalSec %
            GLO_CYCLE_SECONDS;


        if (CycleRelativeSec < 0)
        {
            CycleRelativeSec +=
                GLO_CYCLE_SECONDS;

            --NewCycle;
        }


        //------------------------------------------------------------------
        // Split into new day + seconds-of-day.
        //------------------------------------------------------------------

        const long long NewDayZeroBased =
            CycleRelativeSec /
            GLO_DAY_SECONDS;


        const long long NewTbSeconds =
            CycleRelativeSec %
            GLO_DAY_SECONDS;


        const unsigned short NewDay =
            static_cast<unsigned short>(
                NewDayZeroBased + 1
            );


        //------------------------------------------------------------------
        // tb has 900-second resolution.
        //
        // This matches DecodeEphOrbit():
        //
        //     ((Milliseconds + 450000) / 900000) * 900
        //------------------------------------------------------------------

        unsigned int NewTb =
            static_cast<unsigned int>(
                (
                    NewTbSeconds +
                    GLO_TB_SECONDS / 2
                )
                / GLO_TB_SECONDS
                * GLO_TB_SECONDS
            );


        //------------------------------------------------------------------
        // Rounding can push tb to exactly 86400.
        //
        // Move it to the next day.
        //------------------------------------------------------------------

        long long TbDay =
            static_cast<long long>(
                NewDay
            );


        if (NewTb >= GLO_DAY_SECONDS)
        {
            NewTb = 0;
            ++TbDay;


            if (TbDay > GLO_CYCLE_DAYS)
                TbDay = 1;
        }


        Eph->day =
            static_cast<unsigned short>(
                TbDay
            );

        Eph->tb =
            NewTb;


        //------------------------------------------------------------------
        // tk
        //
        // tk is NOT tb.
        //
        // It is a separate 30-second time-of-day value.
        //------------------------------------------------------------------

        const long long OrigTkSeconds =
            GlonassTkToSeconds(
                Eph->tk
            );


        //------------------------------------------------------------------
        // Determine which day the original tk belongs to relative
        // to the ephemeris tb.
        //
        // tk has no day field.
        //
        // We choose the day nearest to tb.
        //------------------------------------------------------------------

        const long long TbDayForTk =
            static_cast<long long>(
                Eph->day
            );


        const long long TkDay =
            GetTkAbsoluteDay(
                TbDayForTk,
                NewTb,
                OrigTkSeconds
            );


        //------------------------------------------------------------------
        // Absolute tk before actualization.
        //------------------------------------------------------------------

        long long TkTotalSec =
            NewCycle * GLO_CYCLE_SECONDS
            +
            (TkDay - 1) * GLO_DAY_SECONDS
            +
            OrigTkSeconds;


        //------------------------------------------------------------------
        // Apply the same actualization offset.
        //------------------------------------------------------------------

        TkTotalSec += OffsetSec;


        //------------------------------------------------------------------
        // Convert tk back into cycle/day/time-of-day.
        //------------------------------------------------------------------

        long long TkCycle =
            TkTotalSec /
            GLO_CYCLE_SECONDS;

        long long TkCycleSec =
            TkTotalSec %
            GLO_CYCLE_SECONDS;


        if (TkCycleSec < 0)
        {
            TkCycleSec +=
                GLO_CYCLE_SECONDS;

            --TkCycle;
        }


        //------------------------------------------------------------------
        // Extract tk day and seconds-of-day.
        //------------------------------------------------------------------

        long long TkDayZeroBased =
            TkCycleSec /
            GLO_DAY_SECONDS;


        long long NewTkSeconds =
            TkCycleSec %
            GLO_DAY_SECONDS;


        int NewFrameTime =
            static_cast<int>(
                (
                    NewTkSeconds +
                    GLO_FRAME_SECONDS / 2
                )
                / GLO_FRAME_SECONDS
            );


        //------------------------------------------------------------------
        // Handle 23:59:45 rounding.
        //------------------------------------------------------------------

        if (NewFrameTime >= GLO_FRAMES_PER_DAY)
        {
            NewFrameTime = 0;

            ++TkDayZeroBased;

            if (TkDayZeroBased >= GLO_CYCLE_DAYS)
                TkDayZeroBased = 0;
        }


        //------------------------------------------------------------------
        // Encode new tk.
        //------------------------------------------------------------------

        Eph->tk =
            EncodeGlonassTk(
                NewFrameTime
            );
    }
}