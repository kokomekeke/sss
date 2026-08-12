#include ActualizeRinex.h
#include <cstdlib>

//typedef enum { GpsSystem, BdsSystem, GalileoSystem, GlonassSystem, SbasSystem, QzssSystem, NavICSystem } GnssSystem;

bool ParseArgs(int argc, char* argv[], CommandArguments &Arguments) {
    const std::vector<std::string> CommandList = {
		"--help", "-h",	// 0
		"--config", "-c",	// 1
		"--output", "-o",	// 2
		"--validate-only", "-vo",	// 3
	};
	std::string arg;
	int i = 1, index;

	while (i < argc)
	{
		auto it = std::find(CommandList.begin(), CommandList.end(), argv[i]);
		index = (it == CommandList.end()) ? -1 : (it - CommandList.begin()) / 2;
		arg = argv[i];

		switch (index)
		{
		case 0:	// --help
			ShowHelp(argv[0]);
			exit(0);
		case 1:	// --config
			if (i + 1 >= argc || argv[i+1][0] == '-')
			{
				std::cerr << "[ERROR] " << arg << " requires a filename argument\n";
				return false;
			}
			Arguments.ConfigFile = argv[++i];
			break;
		case 2:	// --output
			if (i + 1 >= argc || argv[i+1][0] == '-')
			{
				std::cerr << "[ERROR] " << arg << " requires a filename argument\n";
				return false;
			}
			Arguments.OutputFile = argv[++i];
			break;
		case 3: // input
		    if (i + 1 >= argc || argv[i+1][0] == '-')
			{
				std::cerr << "[ERROR] " << arg << " requires a filename argument\n";
				return false;
			}
			Arguments.InputFile = argv[++i];
			break;

		case 4:	// --validateo-nly
			Arguments.ValidateOnly = true;
			break;

	}

	return true;
}

void ActualizeRinex(UTC_TIME UtcTime, CNavData* NavData, OUTPUT_PARAM OutputParam)
{
	PGPS_EPHEMERIS Pool;
	PGLONASS_EPHEMERIS GlonassPool;
	int Number;

	for (int i = GpsSystem; i <= GlonassSystem; i++)
	{
		GnssSystem system = static_cast<GnssSystem>(i);
		if (!OutputParam.FreqSelect[system])
			continue;
        switch (system) {
            case GlonassSystem:
                GlonassPool = NavData->GetGlonassEphemerisPool();
			    Number = NavData->GetGlonassEphemerisNumber();
			    ProcessGlonassSystem(UtcTime, GlonassPool, Number);
                break;
            case GpsSystem:
                Pool = NavData->GetEphemerisPool(system);
                Number = NavData->GetEphemerisNumber(system);
                ProcessGpsSystem(UtcTime, Pool, Number);
                break;
            case GalileoSystem:

                break;
            case BdsSystem:

                break;
        }
	}
}

void ProcessGlonassSystem (UTC_TIME UtcTime, PGLONASS_EPHEMERIS GlonassPool, int Number) {
    GLONASS_TIME GlonassTime = UtcToGlonassTime(UtcTime);

    for (PGLONASS_EPHEMERIS Glo = GlonassPool;
         Glo < GlonassPool + Number;
         ++Glo)
    {
    }
}

static inline void SplitGpsTime(long long TotalSec, int &Week, int &Sec)
{
	// Helyes, előjeles moduló: TotalSec akár negatív is lehet
	// (ha a szimulációs idő korábbi, mint az eredeti efemerisz-idő),
	// a % operátor C++-ban negatív maradékot adna, ezt korrigáljuk.
	Week = (int)(TotalSec >= 0 ? TotalSec / 604800 : (TotalSec - 604799) / 604800);
	Sec  = (int)(TotalSec - (long long)Week * 604800);
}

void ProcessGpsSystem(UTC_TIME UtcTime, PGPS_EPHEMERIS Pool, int Number)
{
	if (Pool == nullptr || Number <= 0)
		return;

	// A szimulációs kezdő idő GPS-időben, ugyanazzal a konvencióval
	// (UseLeapSecond = FALSE), mint amit a Rinex.cpp DecodeEphParam-ja
	// használ a fájl beolvasásakor (UtcToGpsTime(time, FALSE)) —
	// enélkül a két időbázis nem lenne összemérhető.
	GNSS_TIME SimGpsTime = UtcToGpsTime(UtcTime, FALSE);
	long long SimTotalSec = (long long)SimGpsTime.Week * 604800 + SimGpsTime.MilliSeconds / 1000;

	// Referenciaként az első pool-elem EREDETI toc-ját használjuk.
	// A toc minden formátumnál (LNAV, CNAV, CNAV2) garantáltan
	// megbízhatóan ki van töltve, a toe-vel ellentétben (ahol pl.
	// CNAV esetén Eph->toe = Eph->toc, tehát nem független adat).
	PGPS_EPHEMERIS FirstElement = Pool;
	long long FirstTotalSec = (long long)FirstElement->week * 604800 + FirstElement->toc;

	// Az eltolás EGYETLEN, közös érték, amit minden rekordra alkalmazunk.
	// Ez megőrzi a rekordok egymáshoz viszonyított időbeli távolságát
	// (pl. ha az eredeti fájlban 2 óránként voltak új efemeriszek,
	// az eltolás után is 2 óránként lesznek), csak az egészet
	// a szimulációs kezdő időhöz igazítja.
	long long OffsetSec = SimTotalSec - FirstTotalSec;

	for (PGPS_EPHEMERIS Gps = Pool; Gps < Pool + Number; Gps++)
	{
		long long OrigTocTotal = (long long)Gps->week * 604800 + Gps->toc;
		long long OrigToeTotal = (long long)Gps->week * 604800 + Gps->toe;

		long long NewTocTotal = OrigTocTotal + OffsetSec;
		long long NewToeTotal = OrigToeTotal + OffsetSec;

		int NewWeek, NewToc, NewToeWeek, NewToe;
		SplitGpsTime(NewTocTotal, NewWeek, NewToc);
		SplitGpsTime(NewToeTotal, NewToeWeek, NewToe);

		Gps->toc = NewToc;
		Gps->toe = NewToe;
		Gps->top = NewToe;      // a DecodeEphParam is mindig top = toe-t állít be, itt is így tartjuk konzisztensen
		Gps->week = NewWeek;    // elsődleges hét-mező; a toc és toe azonos héten belül marad a legtöbb esetben
	}
}

int main(int argc, char* argv[]) {
    CommandArguments Arguments;


    Arguments.ConfigFile = "IfGenTest.json"; // Default JSON file
	Arguments.OutputFile = "";
    Arguments.InputFile = "";
	Arguments.ValidateOnly = false;

    if (!ParseArgs(argc, argv, Arguments)) {
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Something wrong happened" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        return -1;
    };




}