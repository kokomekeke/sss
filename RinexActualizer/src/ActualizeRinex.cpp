#include "ActualizeRinex.h"

#include <cstddef>

namespace
{
constexpr long long GPS_WEEK_SECONDS = 604800;

void SplitGpsTime(
    long long TotalSec,
    int& Week,
    int& Sec)
{
    Week = static_cast<int>(
        TotalSec >= 0
            ? TotalSec / GPS_WEEK_SECONDS
            : (TotalSec - GPS_WEEK_SECONDS + 1) /
                  GPS_WEEK_SECONDS
    );

    Sec = static_cast<int>(
        TotalSec -
        static_cast<long long>(Week) *
            GPS_WEEK_SECONDS
    );
}
}

void ActualizeRinex(
    UTC_TIME UtcTime,
    CNavData* NavData,
    OUTPUT_PARAM OutputParam)
{
    if (NavData == nullptr)
        return;

    for (int i = 0;
         i < static_cast<int>(GnssSystem::Count);
         ++i)
    {
        const GnssSystem system =
            static_cast<GnssSystem>(i);

        const std::size_t systemIndex =
            static_cast<std::size_t>(system);

        if (!OutputParam.FreqSelect[systemIndex])
            continue;

        switch (system)
        {
            case GnssSystem::Gps:
            {
                PGPS_EPHEMERIS pool =
                    NavData->GetEphemerisPool(system);

                const int number =
                    NavData->GetEphemerisNumber(system);

                ProcessGpsSystem(
                    UtcTime,
                    pool,
                    number
                );

                break;
            }

            case GnssSystem::Glonass:
            {
                PGLONASS_EPHEMERIS pool =
                    NavData->GetGlonassEphemerisPool();

                const int number =
                    NavData->GetGlonassEphemerisNumber();

                ProcessGlonassSystem(
                    UtcTime,
                    pool,
                    number
                );

                break;
            }

            case GnssSystem::Galileo:
                // TODO
                break;

            case GnssSystem::Bds:
                // TODO
                break;

            case GnssSystem::Sbas:
                // TODO
                break;

            case GnssSystem::Qzss:
                // TODO
                break;

            case GnssSystem::NavIC:
                // TODO
                break;

            case GnssSystem::Count:
                break;
        }
    }
}

void ProcessGpsSystem(
    UTC_TIME UtcTime,
    PGPS_EPHEMERIS Pool,
    int Number)
{
    if (Pool == nullptr || Number <= 0)
        return;

    GNSS_TIME SimGpsTime =
        UtcToGpsTime(UtcTime, FALSE);

    const long long SimTotalSec =
        static_cast<long long>(SimGpsTime.Week) *
            GPS_WEEK_SECONDS +
        SimGpsTime.MilliSeconds / 1000;

    PGPS_EPHEMERIS FirstElement = Pool;

    const long long FirstTotalSec =
        static_cast<long long>(FirstElement->week) *
            GPS_WEEK_SECONDS +
        FirstElement->toc;

    const long long OffsetSec =
        SimTotalSec - FirstTotalSec;

    for (PGPS_EPHEMERIS Gps = Pool;
         Gps < Pool + Number;
         ++Gps)
    {
        const long long OrigTocTotal =
            static_cast<long long>(Gps->week) *
                GPS_WEEK_SECONDS +
            Gps->toc;

        const long long OrigToeTotal =
            static_cast<long long>(Gps->week) *
                GPS_WEEK_SECONDS +
            Gps->toe;

        const long long NewTocTotal =
            OrigTocTotal + OffsetSec;

        const long long NewToeTotal =
            OrigToeTotal + OffsetSec;

        int NewWeek;
        int NewToc;

        int NewToeWeek;
        int NewToe;

        SplitGpsTime(
            NewTocTotal,
            NewWeek,
            NewToc
        );

        SplitGpsTime(
            NewToeTotal,
            NewToeWeek,
            NewToe
        );

        Gps->week = NewWeek;
        Gps->toc = NewToc;
        Gps->toe = NewToe;
        Gps->top = NewToe;
    }
}

void ProcessGlonassSystem(
    UTC_TIME UtcTime,
    PGLONASS_EPHEMERIS Pool,
    int Number)
{
    if (Pool == nullptr || Number <= 0)
        return;

    GLONASS_TIME GlonassTime =
        UtcToGlonassTime(UtcTime);

    for (PGLONASS_EPHEMERIS Glo = Pool;
         Glo < Pool + Number;
         ++Glo)
    {
        // TODO:
        // GLONASS ephemeris actualization
    }
}