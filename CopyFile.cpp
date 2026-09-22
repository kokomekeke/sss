#include <iostream>
#include <boost/filesystem.hpp>
#include <string>
using namespace boost::filesystem;

struct CommandArguments
{
	std::string ConfigFile;
	std::string OutputFile;
	bool MultiThread;
	bool ValidateOnly;
	bool OutputTag;
};

int main(int argc, char* argv[])
{
	CommandArguments args;
	
	// modositani a minimum elemszamra ami a meta filehoz es az iq datafilehoz kell
	if (argc < 2)
  	{
    		std::cout << "Usage: fcopy path/source_file_name path/dest_file_name\n";
    		return 1;
  	}
  	std::string input = argv[1];

  	std::string dest =
      		input.substr(0, input.find_last_of('.')) + ".sigmf_data";

  	copy_file(argv[1], path(dest));
  	std::cout << argv[1] << " " << file_size(argv[1]) << '\n';
  	
	if (!ParseCommandLineArgs(argc, argv, args)) 
	{
		return 1;
	}

	return 0;
}

bool ParseCommandLineArgs(int argc, char* argv[], CommandArguments &Arguments)
{
	const std::vector<std::string> CommandList = {
		"--help", "-h",	// 0
		"--datatype", "-dt",	// 1
		"--output", "-o",	// 2
		"--sample_rate", "-sr",	// 3
		"--author", "-au",	// 4
		"--collection", "-c",	// 5
		"--dataset", "-d",	// 6
		"--description", "-dc", // 7
		"--hw", "-hw", 		// 8
		"--license", "-l",	// 9
		"--data_doi", "-do",	// 10
		"--metadata_only", "-mo", //11
		"--meta_doi", "-md",	// 12
		"--num_channels", "-nc"	// 13
		""  
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
			//
			// implementalni
			//
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
		case 3:	// --validate-only
			Arguments.ValidateOnly = true;
			break;
		case 4:	// --multi-thread
			Arguments.MultiThread = true;
			break;
		case 5:	// --single-thread
			Arguments.MultiThread = false;
			break;
		case 6:	// --tag
			Arguments.OutputTag = true;
			break;
		default:
			std::cout << "[WARNING] Unknown option " << arg << "\n";
		}
		i ++;	// move to next argument
	}

	return true;
}
