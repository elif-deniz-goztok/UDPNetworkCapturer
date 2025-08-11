#include <winsock2.h>
#include <iostream>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    SOCKET sock;
    sockaddr_in serverAddr{}, clientAddr{};
    char buffer[1024];
    int clientAddrLen = sizeof(clientAddr);

    // Config: Local bind address & allowed sender
    const char* localIP   = "127.0.0.1"; // Must be your machine's IP
    const int   localPort = 5555;
    const char* allowedSenderIP   = "127.0.0.1"; // Remote sender's IP
    const int   allowedSenderPort = 59536;            // Remote sender's port

    // 1. Initialize Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << "\n";
        return 1;
    }

    // 2. Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // 3. Bind to specific local IP & port
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(localIP);
    serverAddr.sin_port = htons(localPort);

    if (bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "UDP server listening on " << localIP << ":" << localPort << "\n";
    std::cout << "Only accepting packets from " << allowedSenderIP << ":" << allowedSenderPort << "\n";

    // Create and open a text file
    std::ofstream MyFile(allowedSenderIP + allowedSenderPort, std::ios::binary);

    // 4. Main loop
    while (true) {
        int recvLen = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (recvLen == SOCKET_ERROR) {
            std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
            break;
        }

        buffer[recvLen] = '\0'; // Null-terminate for printing

        // 5. Check if sender matches allowed sender
        std::string senderIP = inet_ntoa(clientAddr.sin_addr);
        int senderPort = ntohs(clientAddr.sin_port);

        if (senderIP == allowedSenderIP && senderPort == allowedSenderPort) {
            std::cout << "From allowed sender " << senderIP << ":" << senderPort
                      << " — " << buffer << "\n";

            MyFile << buffer << "\n";

            // Send acknowledgment
            const char* reply = "Message received";
            sendto(sock, reply, strlen(reply), 0,
                   (struct sockaddr*)&clientAddr, clientAddrLen);
        } else {
            std::cout << "Rejected packet from " << senderIP << ":" << senderPort << "\n";
            // Just ignore, no reply
        }
    }

    // Close the file
    MyFile.close();

    closesocket(sock);
    WSACleanup();
    return 0;
}
