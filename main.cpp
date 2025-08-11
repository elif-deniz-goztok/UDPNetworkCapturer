/*
Okay wonderful. Now make it so that it goes like
"user puts in the input texts"
"pushes the button once"
"screen changes, input boxes and their texts go away"
"the situation of test messages are written on the screen like \"Port 5555: Successful \"\n\"Port 5556: Error: Failed to send test message to port\" "
"after seeing test results, user clicks the button one more time to start the actual capturing"
"user pushes button to stop capturing
"
*/

#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <string>
#pragma comment(lib, "ws2_32.lib")

#define BUTTON_ID 1

// Shared variables
std::atomic<bool> running(false); // Start as false, start listeners only on button
char input_id_c[256], input_1[256], input_2[256];
int input_port_1, input_port_2;
std::string input_id;
CRITICAL_SECTION lastMessageLock; // To protect lastMessage access from multiple threads

struct ListenerParams {
    const char* localIP;
    int localPort;
    const char* allowedSenderIP;
    int allowedSenderPort;
};

std::thread threads[4];
ListenerParams listeners[4];
bool listenersStarted = false;

void udpListener(ListenerParams params)
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        MessageBox(nullptr, "Socket creation failed", "Error", MB_OK | MB_ICONERROR);
        running = false;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(params.localIP);
    serverAddr.sin_port = htons(params.localPort);

    BOOL optVal = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optVal), sizeof(optVal));

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        char errMsg[128];
        sprintf_s(errMsg, "Bind failed on port %d", params.localPort);
        MessageBox(nullptr, errMsg, "Error", MB_OK | MB_ICONERROR);
        closesocket(sock);
        running = false;
        return;
    }

    char buffer[1600];
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    // File name like "received_messages_5555.txt"
    char fileName[64];
    sprintf_s(fileName, "received_messages_%d.txt", params.localPort);
    std::ofstream MyFile(fileName, std::ios::binary);

    if (!MyFile.is_open()) {
        char err[128];
        sprintf_s(err, "Failed to open file for port %d", params.localPort);
        MessageBox(nullptr, err, "Error", MB_OK | MB_ICONERROR);
        running = false;
        return;
    }

    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000; // 0.5 seconds

        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);
        if (selectResult > 0 && FD_ISSET(sock, &readfds)) {
            int recvLen = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                   reinterpret_cast<struct sockaddr*>(&clientAddr), &clientAddrLen);

            if (recvLen == SOCKET_ERROR) {
                std::cout << "recvfrom failed" << std::endl;
                break;
            }

            buffer[recvLen] = '\0';

            std::string senderIP = inet_ntoa(clientAddr.sin_addr);
            int senderPort = ntohs(clientAddr.sin_port);

            std::cout << "Port " << params.localPort << " received from " << senderIP << ":" << senderPort << ": " << buffer << std::endl;

            if (senderIP == params.allowedSenderIP || senderIP == params.localIP) {
                MyFile << buffer << "\n";
                std::cout << params.localPort << ": " << buffer << "\n";
                MyFile.flush();
            }
        }
    }

    closesocket(sock);
    MyFile.close();
}

bool testUdpCommunication(const char* localIP, int ports[], int portCount)
{
    // Create sockets for receiving test messages
    SOCKET recvSockets[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };

    for (int i = 0; i < portCount; ++i) {
        recvSockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (recvSockets[i] == INVALID_SOCKET) {
            MessageBox(nullptr, "Failed to create test recv socket.", "Error", MB_OK | MB_ICONERROR);
            // Cleanup created sockets
            for (int j = 0; j < i; ++j) closesocket(recvSockets[j]);
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(localIP);
        addr.sin_port = htons(ports[i]);

        BOOL optVal = TRUE;
        setsockopt(recvSockets[i], SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optVal), sizeof(optVal));

        if (bind(recvSockets[i], reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            char errMsg[128];
            sprintf_s(errMsg, "Failed to bind test recv socket on port %d", ports[i]);
            MessageBox(nullptr, errMsg, "Error", MB_OK | MB_ICONERROR);
            for (int j = 0; j <= i; ++j) closesocket(recvSockets[j]);
            return false;
        }
    }

    // Create a sending socket (can be shared for all sends)
    SOCKET sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSock == INVALID_SOCKET) {
        MessageBox(nullptr, "Failed to create test send socket.", "Error", MB_OK | MB_ICONERROR);
        for (int i = 0; i < portCount; ++i) closesocket(recvSockets[i]);
        return false;
    }

    // Send test messages to each port
    for (int i = 0; i < portCount; ++i) {
        sockaddr_in destAddr{};
        destAddr.sin_family = AF_INET;
        destAddr.sin_addr.s_addr = inet_addr(localIP);
        destAddr.sin_port = htons(ports[i]);

        const char* testMsg = "TEST_MESSAGE";

        int sendResult = sendto(sendSock, testMsg, static_cast<int>(strlen(testMsg)), 0,
                                reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));

        if (sendResult == SOCKET_ERROR) {
            char errMsg[128];
            sprintf_s(errMsg, "Failed to send test message to port %d", ports[i]);
            MessageBox(nullptr, errMsg, "Error", MB_OK | MB_ICONERROR);
            closesocket(sendSock);
            for (int j = 0; j < portCount; ++j) closesocket(recvSockets[j]);
            return false;
        }
    }

    // Wait for test messages to be received on each socket (with timeout)
    fd_set readfds;
    timeval timeout{};
    timeout.tv_sec = 2;   // 2 seconds timeout to receive each test message
    timeout.tv_usec = 0;

    char buffer[256];
    bool received[4] = { false, false, false, false };

    int remaining = portCount;

    while (remaining > 0) {
        FD_ZERO(&readfds);
        int maxSock = 0;
        for (int i = 0; i < portCount; ++i) {
            if (!received[i]) {
                FD_SET(recvSockets[i], &readfds);
                if (recvSockets[i] > maxSock) maxSock = recvSockets[i];
            }
        }

        int selectRes = select(maxSock + 1, &readfds, nullptr, nullptr, &timeout);
        if (selectRes == 0) {
            // timeout
            break;
        }
        else if (selectRes == SOCKET_ERROR) {
            MessageBox(nullptr, "select() failed during test", "Error", MB_OK | MB_ICONERROR);
            break;
        }
        else {
            for (int i = 0; i < portCount; ++i) {
                if (!received[i] && FD_ISSET(recvSockets[i], &readfds)) {
                    sockaddr_in fromAddr{};
                    int fromLen = sizeof(fromAddr);
                    int recvLen = recvfrom(recvSockets[i], buffer, sizeof(buffer) - 1, 0,
                                           reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
                    if (recvLen > 0) {
                        buffer[recvLen] = '\0';
                        if (strcmp(buffer, "TEST_MESSAGE") == 0) {
                            received[i] = true;
                            remaining--;
                        }
                    }
                }
            }
        }
    }

    // Close all test sockets
    closesocket(sendSock);
    for (int i = 0; i < portCount; ++i) closesocket(recvSockets[i]);

    // Return true only if all received
    for (int i = 0; i < portCount; ++i) {
        if (!received[i]) return false;
    }
    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == BUTTON_ID)
        {
            if (!listenersStarted) {
                // Read inputs on button press
                GetWindowText(GetDlgItem(hwnd, 1001), input_id_c, sizeof(input_id_c));
                GetWindowText(GetDlgItem(hwnd, 1002), input_1, sizeof(input_1));
                GetWindowText(GetDlgItem(hwnd, 1003), input_2, sizeof(input_2));

                input_id = std::string(input_id_c);
                input_port_1 = atoi(input_1);
                input_port_2 = atoi(input_2);

                if (input_id.empty() || input_port_1 == 0 || input_port_2 == 0) {
                    MessageBox(hwnd, "Please enter valid IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                int testPorts[4] = { input_port_1, input_port_1 + 1, input_port_2, input_port_2 + 1 };

                // Run the test first
                if (!testUdpCommunication(input_id_c, testPorts, 4)) {
                    MessageBox(hwnd, "UDP test communication failed. Check your IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                // Initialize listeners with user input
                listeners[0] = { input_id_c, input_port_1, "127.0.0.1", 55362 };
                listeners[1] = { input_id_c, input_port_1 + 1, "127.0.0.1", 55362 };
                listeners[2] = { input_id_c, input_port_2, "127.0.0.1", 55362 };
                listeners[3] = { input_id_c, input_port_2 + 1, "127.0.0.1", 55362 };

                running = true;

                // Start listener threads
                for (int i = 0; i < 4; i++) {
                    threads[i] = std::thread(udpListener, listeners[i]);
                }

                listenersStarted = true;

                MessageBox(hwnd, "Listeners started. Press the button again to stop.", "Info", MB_OK);
            }
            else {
                // Stop listeners on second button press
                running = false;

                // Join threads
                for (int i = 0; i < 4; i++) {
                    if (threads[i].joinable())
                        threads[i].join();
                }

                listenersStarted = false;

                MessageBox(hwnd, "Listeners stopped. Closing application.", "Info", MB_OK);

                PostQuitMessage(0);
            }
        }
        break;

    case WM_DESTROY:
        running = false;

        for (int i = 0; i < 4; i++) {
            if (threads[i].joinable())
                threads[i].join();
        }

        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBox(nullptr, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    InitializeCriticalSection(&lastMessageLock);

    const char CLASS_NAME[] = "MyWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "UDP Listener with GUI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
        nullptr, nullptr, hInstance, nullptr);

    if (hwnd == nullptr) return 0;

    // Create labels (static text) above each input box
    CreateWindow(
        "STATIC", "UDP IP:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 10, 100, 20,
        hwnd, nullptr, hInstance, nullptr);

    CreateWindow(
        "STATIC", "Destination Port 1:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 70, 150, 20,
        hwnd, nullptr, hInstance, nullptr);

    CreateWindow(
        "STATIC", "Destination Port 2:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 130, 150, 20,
        hwnd, nullptr, hInstance, nullptr);

    // Create three edit controls (text inputs)
    CreateWindow(
        "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        20, 40, 160, 20,
        hwnd, reinterpret_cast<HMENU>(1001), hInstance, nullptr);

    CreateWindow(
        "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        20, 100, 160, 20,
        hwnd, reinterpret_cast<HMENU>(1002), hInstance, nullptr);

    CreateWindow(
        "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        20, 160, 160, 20,
        hwnd, reinterpret_cast<HMENU>(1003), hInstance, nullptr);

    // Button to start/stop listening
    CreateWindow(
        "BUTTON", "Start Listening",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 200, 140, 40,
        hwnd, reinterpret_cast<HMENU>(BUTTON_ID), hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteCriticalSection(&lastMessageLock);
    WSACleanup();

    return 0;
}
