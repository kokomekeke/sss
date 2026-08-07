#ifndef ACTUALIZE_RINEX_H
#define ACTUALIZE_RINEX_H

struct CommandArguments
{
	std::string ConfigFile;
	std::string OutputFile;
};

bool ParseArgs(int argc, char* argv[], CommandArguments &Arguments);


#endif