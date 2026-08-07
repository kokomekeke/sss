#include ActualizeRinex.h

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