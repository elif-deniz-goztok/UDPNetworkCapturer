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
std::atomic<bool> running(true);
// std::string lastMessage = "";
CRITICAL_SECTION lastMessageLock; // To protect lastMessage access from multiple threads

struct ListenerParams {
    const char* localIP;
    int localPort;
    const char* allowedSenderIP;
    int allowedSenderPort;
};

void udpListener(ListenerParams params)
{

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        MessageBox(NULL, "Socket creation failed", "Error", MB_OK | MB_ICONERROR);
        running = false;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(params.localIP);
    serverAddr.sin_port = htons(params.localPort);

    BOOL optVal = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

    if (bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        char errMsg[128];
        sprintf_s(errMsg, "Bind failed on port %d", params.localPort);
        MessageBox(NULL, errMsg, "Error", MB_OK | MB_ICONERROR);
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
    // std::ofstream MyFile(fileName, std::ios::binary);
    std::ofstream MyFile(fileName);

    if (!MyFile.is_open()) {
        char err[128];
        sprintf_s(err, "Failed to open file for port %d", params.localPort);
        MessageBox(NULL, err, "Error", MB_OK | MB_ICONERROR);
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

        int selectResult = select(0, &readfds, NULL, NULL, &timeout);
        if (selectResult > 0 && FD_ISSET(sock, &readfds)) {
            int recvLen = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr*)&clientAddr, &clientAddrLen);

            if (recvLen == SOCKET_ERROR) {
                // You may want to handle errors differently per your needs
                std::cout << "recvfrom failed" << std::endl;
                break;
            }

            buffer[recvLen] = '\0';

            std::string senderIP = inet_ntoa(clientAddr.sin_addr);
            int senderPort = ntohs(clientAddr.sin_port);

            std::cout << "Port " << params.localPort << " received from " << senderIP << ":" << senderPort << ": " << buffer << std::endl;

            if (senderIP == params.allowedSenderIP || senderIP == params.localIP) {



                // Write to the file for this port
                MyFile << buffer << "\n";
                std::cout << params.localPort << ": " << buffer << "\n";
                MyFile.flush();

                /*
                // Update shared lastMessage with thread safety
                EnterCriticalSection(&lastMessageLock);
                lastMessage = buffer;
                LeaveCriticalSection(&lastMessageLock);
                */

                /*
                // Optional: send reply
                const char* reply = "Message received";
                sendto(sock, reply, (int)strlen(reply), 0,
                       (struct sockaddr*)&clientAddr, clientAddrLen);
                */
            }
        }
    }

    closesocket(sock);
    MyFile.close();

}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBox(NULL, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        running = false;
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
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    CreateWindow(
        "BUTTON", "Stop and Save",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        140, 70, 120, 40,
        hwnd, (HMENU)BUTTON_ID, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    // Define 4 listeners with ports and allowed sender IP/port as you want
    ListenerParams listeners[4] = {
        {"127.0.0.1", 5555, "127.0.0.1", 61455},
        {"127.0.0.1", 5556, "127.0.0.1", 61455},
        {"127.0.0.1", 6666, "127.0.0.1", 61455},
        {"127.0.0.1", 6667, "127.0.0.1", 61455}
    };


    std::thread threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = std::thread(udpListener, listeners[i]);
    }

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    running = false;  // Stop all threads


    for (int i = 0; i < 4; i++) {
        if (threads[i].joinable())
            threads[i].join();
    }


    DeleteCriticalSection(&lastMessageLock);

    WSACleanup();

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == BUTTON_ID)
        {
            running = false;

            /*
            EnterCriticalSection(&lastMessageLock);
            std::string msgToShow = lastMessage.empty() ? "No messages received yet." : lastMessage;
            LeaveCriticalSection(&lastMessageLock);
            */

            MessageBox(hwnd, "Capturing is stopped", "Last Message", MB_OK | MB_ICONINFORMATION);
            PostQuitMessage(0);


        }
        break;

    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
