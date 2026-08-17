#include "ActualizeRinex.h"

#include <cstdint>

static constexpr long long GPS_WEEK_SECONDS = 604800;

/*
 * Convert absolute GPS seconds into GPS week + seconds-of-week.
 *
 * Handles negative TotalSec as well.
 */
static inline void SplitGpsTime(
    long long TotalSec,
    int& Week,
    int& Sec)
{
    Week = static_cast<int>(
        TotalSec >= 0
            ? TotalSec / GPS_WEEK_SECONDS
            : (TotalSec - (GPS_WEEK_SECONDS - 1)) / GPS_WEEK_SECONDS
    );

    Sec = static_cast<int>(
        TotalSec - static_cast<long long>(Week) * GPS_WEEK_SECONDS
    );
}


/*
 * Main entry point for RINEX actualization.
 */
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

    /*
     * Iterate through currently supported constellations.
     *
     * Later you can extend this range / switch with:
     * Galileo
     * BDS
     * QZSS
     * NavIC
     * SBAS
     */
    for (int i = GpsSystem; i <= GlonassSystem; ++i)
    {
        GnssSystem system = static_cast<GnssSystem>(i);

        if (!OutputParam.FreqSelect[system])
            continue;

        switch (system)
        {
            case BdsSystem:
            case GalileoSystem:
            case GpsSystem:
            {
                Pool = NavData->GetEphemerisPool(system);
                Number = NavData->GetEphemerisNumber(system);

                ProcessGnssSystem(
                    UtcTime,
                    Pool,
                    Number
                );

                break;
            }

            case GlonassSystem:
            {
                GlonassPool = NavData->GetGlonassEphemerisPool();
                Number = NavData->GetGlonassEphemerisNumber();

                ProcessGlonassSystem(
                    UtcTime,
                    GlonassPool,
                    Number
                );

                break;
            }

//            case GalileoSystem:
//                Pool = NavData->GetEphemerisPool(system);
//                Number = NavData->GetEphemerisNumber(system);
//
//                ProcessGnssSystem(
//                    UtcTime,
//                    Pool,
//                    Number
//                );
//
//                break;
//            case BdsSystem:

            case SbasSystem:
            case QzssSystem:
            case NavICSystem:
            default:
                /*
                 * Not implemented yet.
                 */
                break;
        }
    }
}


/*
 * GPS ephemeris actualization.
 */
void ProcessGnssSystem(
    UTC_TIME UtcTime,
    PGPS_EPHEMERIS Pool,
    int Number)
{
    if (Pool == nullptr || Number <= 0)
        return;

    /*
     * Convert simulation UTC to GPS time using the same
     * convention as RINEX decoding.
     */
    GNSS_TIME SimGnssTime = UtcToGpsTime(UtcTime, FALSE);

    const long long SimTotalSec =
        static_cast<long long>(SimGnssTime.Week) * GPS_WEEK_SECONDS
        + SimGnssTime.MilliSeconds / 1000;

    /*
     * Use the first ephemeris TOC as the reference.
     */
    PGPS_EPHEMERIS FirstElement = Pool;

    const long long FirstTotalSec =
        static_cast<long long>(FirstElement->week) * GPS_WEEK_SECONDS
        + FirstElement->toc;

    /*
     * One common offset is applied to the whole ephemeris pool.
     *
     * This preserves the relative timing between ephemerides.
     */
    const long long OffsetSec =
        SimTotalSec - FirstTotalSec;

    for (PGPS_EPHEMERIS Gnss = Pool;
         Gnss < Pool + Number;
         ++Gnss)
    {
        // vhanyadik het vhanyadik mp e 1980(?) ot
        const long long OrigTocTotal =
            static_cast<long long>(Gnss->week) * GPS_WEEK_SECONDS
            + Gnss->toc;

        const long long OrigToeTotal =
            static_cast<long long>(Gnss->week) * GPS_WEEK_SECONDS
            + Gnss->toe;

        const long long NewTocTotal =
            OrigTocTotal + OffsetSec;

        const long long NewToeTotal =
            OrigToeTotal + OffsetSec;

        int NewWeek;
        int NewToc;

        int NewToeWeek;
        int NewToe;

        SplitGnssTime(
            NewTocTotal,
            NewWeek,
            NewToc
        );

        SplitGnssTime(
            NewToeTotal,
            NewToeWeek,
            NewToe
        );

        Gnss->toc = NewToc;
        Gnss->toe = NewToe;

        /*
         * Keep the same convention as DecodeEphParam().
         */
        Gnss->top = NewToe;

        /*
         * The main week field follows TOC.
         */
        Gnss->week = NewWeek;
    }
}


/*
 * GLONASS ephemeris actualization.
 */
void ProcessGlonassSystem(
    UTC_TIME UtcTime,
    PGLONASS_EPHEMERIS GlonassPool,
    int Number)
{
    if (GlonassPool == nullptr || Number <= 0)
        return;

    GLONASS_TIME GlonassTime =
        UtcToGlonassTime(UtcTime);

    /*
     * TODO:
     *
     * Implement GLONASS actualization here.
     */

    for (PGLONASS_EPHEMERIS Glo = GlonassPool;
         Glo < GlonassPool + Number;
         ++Glo)
    {
        (void)Glo;
        (void)GlonassTime;
    }
}