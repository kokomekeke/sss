#include "ActualizeRinex.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>


bool ParseArgs(
    int argc,
    char* argv[],
    CommandArguments& Arguments)
{
    const std::vector<std::string> CommandList =
    {
        "--help", "-h",
        "--config", "-c",
        "--output", "-o",
        "--validate-only", "-vo",
        "--input", "-i"
    };

    int i = 1;

    while (i < argc)
    {
        const std::string arg = argv[i];

        auto it = std::find(
            CommandList.begin(),
            CommandList.end(),
            arg
        );

        if (it == CommandList.end())
        {
            std::cerr
                << "[ERROR] Unknown argument: "
                << arg << '\n';

            return false;
        }

        const int index =
            static_cast<int>(
                (it - CommandList.begin()) / 2
            );

        switch (index)
        {
            case 0:
            {
                ShowHelp(argv[0]);
                return true;
            }

            case 1:
            {
                if (i + 1 >= argc ||
                    argv[i + 1][0] == '-')
                {
                    std::cerr
                        << "[ERROR] "
                        << arg
                        << " requires a filename argument\n";

                    return false;
                }

                Arguments.ConfigFile =
                    argv[++i];

                break;
            }

            case 2:
            {
                if (i + 1 >= argc ||
                    argv[i + 1][0] == '-')
                {
                    std::cerr
                        << "[ERROR] "
                        << arg
                        << " requires a filename argument\n";

                    return false;
                }

                Arguments.OutputFile =
                    argv[++i];

                break;
            }

            case 3:
            {
                Arguments.ValidateOnly = true;
                break;
            }

            case 4:
            {
                if (i + 1 >= argc ||
                    argv[i + 1][0] == '-')
                {
                    std::cerr
                        << "[ERROR] "
                        << arg
                        << " requires a filename argument\n";

                    return false;
                }

                Arguments.InputFile =
                    argv[++i];

                break;
            }

            default:
                return false;
        }

        ++i;
    }

    return true;
}


int main(int argc, char* argv[])
{
    CommandArguments Arguments;

    Arguments.ConfigFile = "IfGenTest.json";
    Arguments.OutputFile = "";
    Arguments.InputFile = "";
    Arguments.ValidateOnly = false;

    if (!ParseArgs(argc, argv, Arguments))
    {
        std::cout
            << std::string(50, '-')
            << '\n';

        std::cout
            << "Something wrong happened"
            << '\n';

        std::cout
            << std::string(50, '-')
            << '\n';

        return -1;
    }

    /*
     * TODO:
     *
     * Load configuration
     * Load RINEX
     * Create CNavData
     * Create OUTPUT_PARAM
     * Call ActualizeRinex(...)
     */

    return 0;
}