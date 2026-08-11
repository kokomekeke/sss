#include ActualizeRinex.h

typedef enum { GpsSystem, BdsSystem, GalileoSystem, GlonassSystem, SbasSystem, QzssSystem, NavICSystem } GnssSystem;

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

		if (system == GlonassSystem)
		{
			GlonassPool = NavData->GetGlonassEphemerisPool();
			Number = NavData->GetGlonassEphemerisNumber();
			ProcessGlonassSystem(UtcTime, GlonassPool, Number);
		}
		else
		{
			Pool = NavData->GetEphemerisPool(system);
			Number = NavData->GetEphemerisNumber(system);
			ProcessGnssSystem(UtcTime, Pool, Number);
		}
	}
}

void ProcessGlonassSystem (UTC_TIME UtcTime, PGLONASS_EPHEMERIS GlonassPool, int Number) {
    GLONASS_TIME GlonassTime = UtcToGlonassTime(UtcTime);

}

void ProcessGnssSystem (UTC_TIME UtcTime, PGPS_EPHEMERIS Pool, int Number) {
    GNSS_TIME GpsTime = UtcToGpsTime(UtcTime, false)

    double week = GpsTime.Week;

    double tow = GpsTime.MilliSeconds / 1000.0;
}

int main(int argc, char* argv[]) {
    CommandArguments Arguments;


    Arguments.ConfigFile = "IfGenTest.json"; // Default JSON file
	Arguments.OutputFile = "";
    Arguments.InputFile = "";
	Arguments.ValidateOnly = false;

    if (!ParseArgs(argc, argv, Arguments)) {
        std::cout << std::string(50, '-') << std::endl;
        std::cout << std::string('Something wrong happened') << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        return -1;
    };




}