#include <gnss/gnsslib.h>

#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout
            << "usage: " << argv[0]
            << " <config.json> [max_elements]\n";
        return 0;
    }

    IfGenOptions options;
    options.configFile = argv[1];

    // Alapból csak 20 elemet írunk ki,
    // hogy ne öntsük tele a terminált.
    std::size_t maxElements = 20;

    if (argc >= 3) {
        maxElements =
            std::strtoull(argv[2], nullptr, 10);
    }

    IfGenEngine eng;

    try {
        eng.Init(options);
    }
    catch (const std::exception& e) {
        std::cerr
            << "Init failed: "
            << e.what()
            << "\n";

        return 1;
    }

    const IfGenInfo& info = eng.Info();

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "GNSSLib Stream Element Test\n";
    std::cout << "========================================\n";

    std::cout
        << "Sample rate : "
        << info.sampleRateHz / 1e6
        << " MHz\n";

    std::cout
        << "Center freq : "
        << info.centerFreqHz / 1e6
        << " MHz\n";

    std::cout
        << "Channels    : "
        << info.nChannels
        << "\n";

    std::cout << "----------------------------------------\n";

    std::size_t totalElements = 0;

    eng.Stream(
        [&](const std::complex<float>* s,
            std::size_t n) -> bool
        {
            for (std::size_t i = 0; i < n; ++i) {

                if (totalElements < maxElements) {

                    std::cout
                        << "Element #"
                        << totalElements
                        << "\n";

                    std::cout
                        << "  address      = "
                        << static_cast<const void*>(&s[i])
                        << "\n";

                    std::cout
                        << "  type         = std::complex<float>\n";

                    std::cout
                        << "  size         = "
                        << sizeof(s[i])
                        << " bytes\n";

                    std::cout
                        << "  I            = "
                        << s[i].real()
                        << "\n";

                    std::cout
                        << "  Q            = "
                        << s[i].imag()
                        << "\n";

                    std::cout << "\n";
                }

                ++totalElements;

                if (totalElements >= maxElements) {
                    return false;
                }
            }

            return true;
        }
    );

    eng.Close();

    std::cout << "----------------------------------------\n";

    std::cout
        << "Elements inspected: "
        << (totalElements < maxElements
                ? totalElements
                : maxElements)
        << "\n";

    std::cout
        << "sizeof(complex<float>): "
        << sizeof(std::complex<float>)
        << " bytes\n";

    std::cout << "========================================\n";

    return 0;
}
