// simple program to turn a list of single floating point numbers into its representation in memory
// mixed type structures would require padding

#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <string>
#include <iomanip>
#include <winsock.h>

int main(size_t argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        float f = std::stof(std::string(argv[i]));
        uint32_t bitrep;
        std::memcpy(&bitrep, &f, 4);
        bitrep = htonl(bitrep); // something about endianness
        std::cout << std::hex << std::setfill('0') << std::setw(8) << bitrep << ' ';
    }
    std::cout << std::endl;
    return 0;
}
