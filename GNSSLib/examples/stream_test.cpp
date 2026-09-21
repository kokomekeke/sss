#include <gnss/gnsslib.h>

#include <complex>
#include <cstddef>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "usage: "
                  << argv[0]
                  << " <config.json>\n";
        return 0;
    }

    IfGenOptions options;
    options.configFile = argv[1];

    IfGenEngine eng;

    try {
        eng.Init(options);
    }
    catch (const std::exception& e) {
        std::cerr << "Init failed: " << e.what() << "\n";
        return 1;
    }

    const IfGenInfo& info = eng.Info();

    std::cout << "========================================\n";
    std::cout << "GNSSLib Element-by-Element Test\n";
    std::cout << "========================================\n";

    std::cout << "Sample rate : "
              << info.sampleRateHz / 1e6
              << " MHz\n";

    std::cout << "Center freq : "
              << info.centerFreqHz / 1e6
              << " MHz\n";

    std::cout << "Channels    : "
              << info.nChannels
              << "\n";

    std::cout << "sizeof(complex<float>) = "
              << sizeof(std::complex<float>)
              << " bytes\n";

    std::cout << "----------------------------------------\n";

    std::size_t elementNumber = 0;

    // A Read() alapú Stream ágat használjuk.
    constexpr std::size_t chunk = 4000;

    eng.Stream(
        [&](const std::complex<float>* samples,
            std::size_t n) -> bool
        {
            for (std::size_t i = 0; i < n; ++i) {

                const std::complex<float>& sample = samples[i];

                std::cout
                    << "Element #" << elementNumber
                    << " | size=" << sizeof(sample)
                    << " bytes"
                    << " | I=" << sample.real()
                    << " | Q=" << sample.imag()
                    << "\n";

                ++elementNumber;
            }

            // Első blokk után álljunk meg.
            return false;
        },
        chunk
    );

    eng.Close();

    std::cout << "----------------------------------------\n";
    std::cout << "Received elements: "
              << elementNumber
              << "\n";

    return 0;
}
