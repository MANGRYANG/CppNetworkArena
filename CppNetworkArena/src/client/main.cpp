#include <iostream>

#include "NetworkTypes.h"

int main(void)
{
    constexpr cna::RoomId invalidRoomId = 0;

    std::cout << "CppNetworkArena GameClient starting...\n";
    std::cout << "Invalid Room ID: " << invalidRoomId << '\n';
    std::cout << "Client bootstrap complete.\n";

    return 0;
}