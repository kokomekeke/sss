// Source - https://stackoverflow.com/a/48871008
// Posted by Richard
// Retrieved 2026-09-14, License - CC BY-SA 3.0

#include <exception>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main()
{
    fs::path sourceFile = "/home/sagax/Documents/simtime.md";
    fs::path targetParent = "/home/sagax/Documents/simtime2.md";
    auto target = targetParent / sourceFile.filename(); // sourceFile.filename() returns "sourceFile.ext".

    try // If you want to avoid exception handling, then use the error code overload of the following functions.
    {
        fs::create_directories(targetParent); // Recursively create target directory if not existing.
        fs::copy_file(sourceFile, target, fs::copy_options::overwrite_existing);
    }
    catch (std::exception& e) // Not using fs::filesystem_error since std::bad_alloc can throw too.  
    {
        std::cout << e.what();
    }
}
