/*
Added the source destination input to the GUI. Should test if that part works.
Must change test algo, tests use src_addr and dst_addr as same rn so it never fails.
 */

#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <string>
#pragma comment(lib, "ws2_32.lib")

#define BUTTON1_ID 101
#define BUTTON2_ID 102
#define BUTTON3_ID 103

// Scene 1 controls
HWND hText1, hInput1;
HWND hText2, hInput2;
HWND hText3, hInput3;
HWND hText4, hInput4;
HWND hButton1;

// Scene 2 controls (all EDIT boxes instead of static)
HWND test_text_1, test_text_2, test_text_3, test_text_4;
HWND hButton2;

// Scene 3 controls
HWND capturing_text;
HWND hButton3;

// Shared variables
std::atomic<bool> running(false); // Start as false, start listeners only on button
char input_dst_ip[256], input_src_ip[256], input_1[256], input_2[256];
int input_port_1, input_port_2;
std::string input_dst_ip_str, input_src_ip_str;
CRITICAL_SECTION lastMessageLock; // To protect lastMessage access from multiple threads

struct ListenerParams {
    const char* localIP;
    int localPort;

    const char* allowedSenderIP;
};

std::thread threads[4];
ListenerParams listeners[4];
bool listenersStarted = false;

void ShowScene1(BOOL show)
{
    ShowWindow(hText1, show);
    ShowWindow(hInput1, show);
    ShowWindow(hText2, show);
    ShowWindow(hInput2, show);
    ShowWindow(hText3, show);
    ShowWindow(hInput3, show);
    ShowWindow(hText4, show);
    ShowWindow(hInput4, show);
    ShowWindow(hButton1, show);
}

void ShowScene2(BOOL show)
{
    ShowWindow(test_text_1, show);
    ShowWindow(test_text_2, show);
    ShowWindow(test_text_3, show);
    ShowWindow(test_text_4, show);
    ShowWindow(hButton2, show);
}

void ShowScene3(BOOL show)
{
    ShowWindow(capturing_text, show);
    ShowWindow(hButton3, show);
}

void change_test_text(int port_num, int ports[], std::string test_message) {
    std::string full_message = "Port " + std::to_string(ports[port_num]) + ": " + test_message;

    switch (port_num) {
        case 0:
            SetWindowText(test_text_1, full_message.c_str());
        break;
        case 1:
            SetWindowText(test_text_2, full_message.c_str());
        break;
        case 2:
            SetWindowText(test_text_3, full_message.c_str());
        break;
        case 3:
            SetWindowText(test_text_4, full_message.c_str());
        break;
        default:
            break;
    }
}


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

    // src ip ++
    // src port --
    // dest ip ++
    // dest port ++

    if (bind(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        char errMsg[128];

        // change_test_text(0, [params.localPort], "Bind failed on port".c_str());
        sprintf_s(errMsg, "Bind failed on port %d", params.localPort);
        MessageBox(nullptr, errMsg, "Error", MB_OK | MB_ICONERROR);
        closesocket(sock);
        running = false;
        return;
    }

    char buffer[1600];
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    char fileName[64];
    sprintf_s(fileName, "%s_%d.bin", params.localIP,params.localPort);
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

            std::string senderIP = inet_ntoa(clientAddr.sin_addr);

            std::cout << params.localPort << "\n";

            if (senderIP == params.allowedSenderIP) {
                MyFile.write(buffer, recvLen);
                    // std::cout << params.localPort << "\n";
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
            change_test_text(i, ports,"Failed to create test recv socket.");

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
            change_test_text(i, ports,"Failed to bind test recv socket.");
            for (int j = 0; j <= i; ++j) closesocket(recvSockets[j]);
            return false;
        }
    }

    // Create a sending socket (can be shared for all sends)
    SOCKET sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSock == INVALID_SOCKET) {
        for (int i = 0; i < portCount; i++) {
            change_test_text(i, ports,"Failed to create test send socket.");
        }
        for (int i = 0; i < portCount; ++i) closesocket(recvSockets[i]);
        return false;
    }

    // Send test messages to each port
    for (int i = 0; i < portCount; ++i) {
        sockaddr_in destAddr{};
        destAddr.sin_family = AF_INET;
        destAddr.sin_addr.s_addr = inet_addr(localIP);

        // BUNUN BÖYLE OLMAMASI LAZIM
        destAddr.sin_port = htons(ports[i]);

        const char* testMsg = "TEST_MESSAGE";

        int sendResult = sendto(sendSock, testMsg, static_cast<int>(strlen(testMsg)), 0,
                                reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));

        if (sendResult == SOCKET_ERROR) {
            change_test_text(i, ports,"Failed to send test message to port.");
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
            for (int i = 0; i < portCount; ++i) {
                change_test_text(i, ports,"select() failed during test.");
            }
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
                            change_test_text(i, ports,"Tests successful.");
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
    case WM_CREATE:
    {
        // Scene 1: Text labels as EDIT boxes, plus input boxes as EDIT boxes
        // Since you want no static texts, we make them all EDIT boxes
        hText1 = CreateWindow("EDIT", "UDP Source IP:", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 10, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput1 = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 40, 150, 20, hwnd, reinterpret_cast<HMENU>(201), nullptr, nullptr);

        hText2 = CreateWindow("EDIT", "UDP Destination IP:", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 70, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput2 = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 100, 150, 20, hwnd, reinterpret_cast<HMENU>(202), nullptr, nullptr);

        hText3 = CreateWindow("EDIT", "UDP Port 1:", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 130, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput3 = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 160, 150, 20, hwnd, reinterpret_cast<HMENU>(203), nullptr, nullptr);

        hText4 = CreateWindow("EDIT", "UDP Port 2", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 190, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput4 = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 20, 220, 150, 20, hwnd, reinterpret_cast<HMENU>(204), nullptr, nullptr);

        hButton1 = CreateWindow("BUTTON", "Test Connections", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 250, 150, 20, hwnd, reinterpret_cast<HMENU>(BUTTON1_ID), nullptr, nullptr);

        // Scene 2: all texts as EDIT boxes (no static)
        test_text_1 = CreateWindow("EDIT", "Test port", WS_CHILD | WS_BORDER | ES_LEFT, 20, 20, 240, 20, hwnd, nullptr, nullptr, nullptr);
        test_text_2 = CreateWindow("EDIT", "Test port", WS_CHILD | WS_BORDER | ES_LEFT, 20, 50, 240, 20, hwnd, nullptr, nullptr, nullptr);
        test_text_3 = CreateWindow("EDIT", "Test port", WS_CHILD | WS_BORDER | ES_LEFT, 20, 80, 240, 20, hwnd, nullptr, nullptr, nullptr);
        test_text_4 = CreateWindow("EDIT", "Test port", WS_CHILD | WS_BORDER | ES_LEFT, 20, 110, 240, 20, hwnd, nullptr, nullptr, nullptr);

        hButton2 = CreateWindow("BUTTON", "Start Capturing", WS_CHILD | BS_PUSHBUTTON, 110, 150, 100, 30, hwnd, reinterpret_cast<HMENU>(BUTTON2_ID), nullptr, nullptr);

        // Scene 3: text 8 as EDIT box and button
        capturing_text = CreateWindow("EDIT", "Capturing UDP Packets", WS_CHILD | WS_BORDER | ES_LEFT, 20, 50, 240, 20, hwnd, nullptr, nullptr, nullptr);
        hButton3 = CreateWindow("BUTTON", "Stop Capturing", WS_CHILD | BS_PUSHBUTTON, 110, 100, 100, 30, hwnd, reinterpret_cast<HMENU>(BUTTON3_ID), nullptr, nullptr);

        // Show only scene 1 at start
        ShowScene1(TRUE);
        ShowScene2(FALSE);
        ShowScene3(FALSE);

        break;
    }

    case WM_COMMAND:{
        switch (LOWORD(wParam)) {
            case BUTTON1_ID: {
                ShowScene1(FALSE);
                ShowScene2(TRUE);
                ShowScene3(FALSE);

                // Read inputs on button press
                GetWindowText(GetDlgItem(hwnd, 201), input_dst_ip, sizeof(input_dst_ip));
                GetWindowText(GetDlgItem(hwnd, 202), input_src_ip, sizeof(input_src_ip));
                GetWindowText(GetDlgItem(hwnd, 203), input_1, sizeof(input_1));
                GetWindowText(GetDlgItem(hwnd, 204), input_2, sizeof(input_2));

                input_dst_ip_str = std::string(input_dst_ip);
                input_src_ip_str = std::string(input_src_ip);
                input_port_1 = atoi(input_1);
                input_port_2 = atoi(input_2);

                if (input_dst_ip_str.empty() || input_src_ip_str.empty() || input_port_1 == 0 || input_port_2 == 0) {
                    MessageBox(hwnd, "Please enter valid IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                int testPorts[4] = { input_port_1, input_port_1 + 1, input_port_2, input_port_2 + 1 };

                // Run the test first
                // Burada testimin sonuçlarını göstermem lazım [test_text_1, hTex7] ile
                if (!testUdpCommunication(input_src_ip, testPorts, 4)) {
                    MessageBox(hwnd, "UDP test communication failed. Check your IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                break;
            }

            case BUTTON2_ID:
                {
                ShowScene1(FALSE);
                ShowScene2(FALSE);
                ShowScene3(TRUE);

                /*
                struct ListenerParams {
                    const char* localIP;
                    int localPort;
                    const char* allowedSenderIP;
                    }
                 */

                // Initialize listeners with user input
                listeners[0] = { input_src_ip, (input_port_1), input_dst_ip};
                listeners[1] = { input_src_ip, (input_port_1 + 1), input_dst_ip};
                listeners[2] = { input_src_ip, (input_port_2), input_dst_ip};
                listeners[3] = { input_src_ip, (input_port_2 + 1), input_dst_ip};

                running = true;

                // Start listener threads
                for (int i = 0; i < 4; i++) {
                    threads[i] = std::thread(udpListener, listeners[i]);
                }
                listenersStarted = true;
                // MessageBox(hwnd, "Listeners started. Press the button again to stop.", "Info", MB_OK);
                break;
                }

            case BUTTON3_ID: {
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
                break;
            }
            break;
        }
        break;
    }

    case WM_DESTROY:
        {
        running = false;

        for (int i = 0; i < 4; i++) {
            if (threads[i].joinable())
                threads[i].join();
        }

        PostQuitMessage(0);
        return 0;
        }
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

    const char CLASS_NAME[] = "SceneSwitchWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "UDP Network Capturer",
        WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 350,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
        return 0;

    ShowScene1(TRUE);
    ShowScene2(FALSE);
    ShowScene3(FALSE);
    
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
