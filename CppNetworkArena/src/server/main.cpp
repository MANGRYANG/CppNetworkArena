#include <iostream>

#include "NetworkTypes.h"

int main(void)
{
    constexpr cna::PlayerId invalidPlayerId = 0;

    std::cout << "CppNetworkArena GameServer starting...\n";
    std::cout << "Invalid Player ID: " << invalidPlayerId << '\n';
    std::cout << "Server bootstrap complete.\n";

    return 0;
}