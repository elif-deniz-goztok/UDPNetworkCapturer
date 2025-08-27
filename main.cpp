#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <string>
#include <utility>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <shobjidl.h> // For IFileDialog (folder picker)
#include <vector>
#include "resources.h"
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// --- Control IDs for buttons and input fields ---
#define BUTTON1_ID 101
#define BUTTON2_ID 102
#define BUTTON3_ID 103
#define BUTTON4_ID 104
#define INPUT1_ID 201
#define INPUT2_ID 202
#define INPUT3_ID 203
#define INPUT4_ID 204
#define INPUT5_ID 205

// --- Scene 1 (input form controls) ---
HWND hText1, hInput1;
HWND hText2, hInput2;
HWND hText3, hInput3;
HWND hText4, hInput4;
HWND hText5, hInput5;
HWND ipText;
HWND hButtonTest;

// --- Scene 2 (connection test results) ---
HWND testText1, testText2, testText3, testText4;
HWND hButtonStartAnyway;
HWND hButton2;

// --- Scene 3 (capture info & stop button) ---
HWND infoText;
HWND capturingText;
HWND hButtonStop;

// --- Shared variables between scenes/threads ---
std::atomic<bool> running(false); // Controls listener threads
char inputDstIP[256], inputSrcIP[256], input1[256], input2[256], inputTGSNodeID[256];
int inputPort1, inputPort2, testPorts[4];
bool testsPassed = true;
std::string inputStrDstIP, inputStrSrcIP;
std::wstring ethernetAddr, result, full_InfoText, TGSNodeID, chosenOutputDirectory;
SYSTEMTIME sys_time;

// --- Parameters for each listener thread ---
struct ListenerParams {
    const char* localIP = nullptr;
    int localPort = -1;
    const char* allowedSenderIP = nullptr;
    bool startedRecording = false;
    std::wstring fileName;

    ListenerParams() = default;
    ListenerParams(const char* ip, int port, const char* allowedIP, bool recording, std::wstring fName)
        : localIP(ip), localPort(port), allowedSenderIP(allowedIP),
          startedRecording(recording), fileName(std::move(fName)) {}
};

// Listener threads and their configs
std::thread threads[4];
ListenerParams listeners[4];
bool listenersStarted = false;
bool foundAddress = false;

// ------------------- UI Scene Toggles -------------------

void ShowScene1(BOOL show) {
    // Show or hide scene 1 controls (IP, ports, Node ID, Test button)
    ShowWindow(hText1, show);
    ShowWindow(hInput1, show);
    ShowWindow(hText2, show);
    ShowWindow(hInput2, show);
    ShowWindow(hText3, show);
    ShowWindow(hInput3, show);
    ShowWindow(hText4, show);
    ShowWindow(hInput4, show);
    ShowWindow(hText5, show);
    ShowWindow(hInput5, show);
    ShowWindow(hButtonTest, show);
    if (foundAddress) ShowWindow(ipText, show);
}

void ShowScene2(BOOL show) {
    // Show or hide test result labels + Start button
    ShowWindow(testText1, show);
    ShowWindow(testText2, show);
    ShowWindow(testText3, show);
    ShowWindow(testText4, show);
    ShowWindow(hButton2, show);
    if (!testsPassed) ShowWindow(hButtonStartAnyway, show);
}

void ShowScene3(BOOL show) {
    // Show or hide capturing info + Stop button
    ShowWindow(infoText, show);
    ShowWindow(capturingText, show);
    ShowWindow(hButtonStop, show);
}

// ------------------- Utility Functions -------------------

std::wstring findEthernetAddress() {
    // Finds the IPv4 address of the "Ethernet" adapter
    ULONG outBufLen = 15000;
    auto adapterAddresses = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(outBufLen));
    if (!adapterAddresses) return L"";

    ULONG retVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &outBufLen);

    if (retVal == ERROR_BUFFER_OVERFLOW) {
        // Retry with larger buffer
        free(adapterAddresses);
        adapterAddresses = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(outBufLen));
        if (!adapterAddresses) return L"";
        retVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &outBufLen);
    }

    if (retVal == NO_ERROR) {
        // Look for Ethernet adapter and return its IPv4 address
        for (PIP_ADAPTER_ADDRESSES adapter = adapterAddresses; adapter; adapter = adapter->Next) {
            if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD &&
                wcscmp(adapter->FriendlyName, L"Ethernet") == 0) {
                for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress;
                     unicast; unicast = unicast->Next) {
                    if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                        auto* ipv4 = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                        std::string ipStr = inet_ntoa(ipv4->sin_addr);
                        result = std::wstring(ipStr.begin(), ipStr.end());
                        break;
                    }
                }
            }
            if (!result.empty()) break;
        }
    }
    free(adapterAddresses);
    return result; // Empty if not found
}

std::wstring ChooseOutputFolder(HWND hwnd) {
    // Open a folder picker dialog, return chosen directory as wstring
    IFileDialog* pfd = nullptr;
    std::wstring folderPath;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return L"";

    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                          IID_IFileDialog, reinterpret_cast<void**>(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
        pfd->SetTitle(L"Select folder for recording files:");

        hr = pfd->Show(hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem;
            hr = pfd->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    folderPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }

        /*
        if (SUCCEEDED(pfd->Show(hwnd))) {
            IShellItem* pItem;
            if (SUCCEEDED(pfd->GetResult(&pItem))) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    folderPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
         */
        pfd->Release();
    }
    CoUninitialize();
    return folderPath;
}

bool inputValuesPassTests() {
    // Validate entered IPs and ports
    in_addr addr1{}, addr2{};
    if (inputStrSrcIP.empty() || inputStrDstIP.empty()
        || inputStrSrcIP.find('-') != std::string::npos
        || inputStrDstIP.find('-') != std::string::npos
        || inputStrSrcIP == "0.0.0.0" || inputStrDstIP == "0.0.0.0"
        || inputPort1 > 65535 || inputPort2 > 65535
        || inputPort1 <= 0 || inputPort2 <= 0
        || (inet_pton(AF_INET, inputSrcIP, &addr1) != 1)
        || (inet_pton(AF_INET, inputDstIP, &addr2) != 1)
        || strlen(inputTGSNodeID) == 0
        ) {
            return false;
        }
    return true;
}

void changeTestText(const std::string& inputStrDstIP, int port_num, int ports[], const std::string& test_message) {
    std::string full_message;
    if (inputStrDstIP.empty()) {
        full_message = "Test port";
    } else {
        full_message = inputStrDstIP + ":" + std::to_string(ports[port_num]) + " -> " + test_message;
    }

    switch (port_num) {
        case 0:
            SetWindowText(testText1, full_message.c_str());
        break;
        case 1:
            SetWindowText(testText2, full_message.c_str());
        break;
        case 2:
            SetWindowText(testText3, full_message.c_str());
        break;
        case 3:
            SetWindowText(testText4, full_message.c_str());
        break;
        default:
            break;
    }
}

// ------------------- UDP Connection Functions -------------------

bool testUdpCommunication(const char* localIP, int ports[], int portCount)
{

    for (int i = 0; i < portCount; i++) {
        for (int j = 0; j < portCount; j++) {
            if ((i != j) && (ports[i] == ports[j])) {
                changeTestText(localIP, i, ports,"Ports crashed.");
                changeTestText(localIP, j, ports,"Ports crashed.");
                return false;
            }
        }
    }

    // Create sockets for receiving test messages
    SOCKET recvSockets[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };

    for (int i = 0; i < portCount; i++) {
        recvSockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (recvSockets[i] == INVALID_SOCKET) {
            changeTestText(localIP, i, ports,"Failed to create test recv socket.");

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
            changeTestText(localIP, i, ports,"Failed to bind test recv socket 2.");
            for (int j = 0; j <= i; ++j) closesocket(recvSockets[j]);
            return false;
        }
    }

    // Wait for test messages to be received on each socket (with timeout)
    fd_set readfds;
    timeval timeout{};
    timeout.tv_sec = 2;   // 2 seconds timeout to receive each test message
    timeout.tv_usec = 0;

    char buffer[1600];
    bool received[4] = { false, false, false, false };

    int remaining = portCount;

    while (remaining > 0) {
        FD_ZERO(&readfds);
        int maxSock = 0;
        for (int i = 0; i < portCount; i++) {
            if (!received[i]) {
                FD_SET(recvSockets[i], &readfds);
                if (recvSockets[i] > maxSock) maxSock = recvSockets[i];
            }
        }

        int selectRes = select(maxSock + 1, &readfds, nullptr, nullptr, &timeout);
        if (selectRes == 0) {
            // timeout
            for (int i = 0; i < portCount; i++) {
                changeTestText(localIP, i, ports,"Failed with timeout.");
            }
            break;
        }
        if (selectRes == SOCKET_ERROR) {
            for (int i = 0; i < portCount; i++) {
                changeTestText(localIP, i, ports,"select() failed during test.");
            }
            break;
        }

        for (int i = 0; i < portCount; i++) {
            if (!received[i] && FD_ISSET(recvSockets[i], &readfds)) {
                sockaddr_in fromAddr{};
                int fromLen = sizeof(fromAddr);
                int recvLen = recvfrom(recvSockets[i], buffer, sizeof(buffer) - 1, 0,
                                       reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
                if (recvLen > 0) {
                    received[i] = true;
                    changeTestText(localIP, i, ports,"Tests successful.");
                    remaining--;
                } else {
                    changeTestText(localIP, i, ports,"RecvLen < 0.");
                    remaining--;
                }
            }
        }
    }

    // Close all test sockets
    for (int i = 0; i < portCount; ++i) closesocket(recvSockets[i]);

    // Return true only if all received
    for (int i = 0; i < portCount; ++i) {
        if (!received[i]) return false;
    }
    return true;
}

void udpListener(ListenerParams params, HWND hwnd)
{
    SOCKET sock = INVALID_SOCKET;
    std::ofstream outputFile;
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    // Pre-parse allowed sender IP
    in_addr allowedAddr{};
    if (inet_pton(AF_INET, params.allowedSenderIP, &allowedAddr) != 1) {
        MessageBox(hwnd, "Invalid allowedSenderIP", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Keep trying until we can start recording
    while (!params.startedRecording && running) {
        // Create socket and bind
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(params.localIP);
        serverAddr.sin_port = htons(params.localPort);

        BOOL optVal = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optVal), sizeof(optVal));

        if (bind(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            return;
        }

        // Example params.fileName = "ST1_MetaData"
        // Example fullFileName = "ST1_MetaData_TGSNodeID_Year_Month_Day_Hour_Minute.bin"
        std::wstring fullFileName = (chosenOutputDirectory + L"\\" + params.fileName + L"_" +
            TGSNodeID + L"_" +
            std::to_wstring(sys_time.wYear) + L"_" +
            std::to_wstring(sys_time.wMonth) + L"_" +
            std::to_wstring(sys_time.wDay) + L"_" +
            std::to_wstring(sys_time.wHour) + L"_" +
            std::to_wstring(sys_time.wMinute) +
            L".bin");

        outputFile.open(fullFileName.c_str(), std::ios::binary);
        if (!outputFile.is_open()) {
            closesocket(sock);
            return;
        }

        params.startedRecording = true;
    }

    // --- New buffered write setup ---
    const size_t BUFFER_SIZE = 1024 * 1024 * 2; // 2 MB
    std::vector<char> bufferCache;
    bufferCache.reserve(BUFFER_SIZE);
    auto lastFlushTime = std::chrono::steady_clock::now();

    // Recording loop
    while (running && params.startedRecording) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 0.1 second

        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);
        if (selectResult > 0 && FD_ISSET(sock, &readfds)) {
            char buffer[1600];
            int recvLen = recvfrom(sock, buffer, sizeof(buffer), 0,
                                   reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

            if (recvLen > 0 && clientAddr.sin_addr.s_addr == allowedAddr.s_addr) {

                // Add packet to cache
                bufferCache.insert(bufferCache.end(), buffer, buffer + recvLen);

                // Flush if buffer is full
                if (bufferCache.size() >= BUFFER_SIZE) {
                    outputFile.write(bufferCache.data(), bufferCache.size());
                    outputFile.flush();
                    bufferCache.clear();
                    lastFlushTime = std::chrono::steady_clock::now();
                }
            }
        }

        // Periodic flush (every 1 second)
        auto now = std::chrono::steady_clock::now();
        if (!bufferCache.empty() &&
            std::chrono::duration_cast<std::chrono::seconds>(now - lastFlushTime).count() >= 1) {
            outputFile.write(bufferCache.data(), bufferCache.size());
            outputFile.flush();
            bufferCache.clear();
            lastFlushTime = now;
        }
    }

    // Write any remaining data
    if (!bufferCache.empty()) {
        outputFile.write(bufferCache.data(), bufferCache.size());
        outputFile.flush();
    }

    if (sock != INVALID_SOCKET) closesocket(sock);
    if (outputFile.is_open()) outputFile.close();
}

// ------------------- GUI Functions -------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // The function that processes messages sent to the window.

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // WM_CREATE sets up the texts and scenes when the application is opened.
        // This scenario happens is when the window is created.

        // Scene 1: Text labels as STATIC boxes, plus input boxes as EDIT boxes
        hText1 = CreateWindowW(L"STATIC", L"UDP Source IP:", WS_VISIBLE | WS_CHILD | ES_LEFT,
                               50, 20, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput1 = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                                50, 50, 150, 20, hwnd, reinterpret_cast<HMENU>(INPUT1_ID), nullptr, nullptr);

        hText2 = CreateWindowW(L"STATIC", L"UDP Destination IP:", WS_VISIBLE | WS_CHILD | ES_LEFT,
                               50, 90, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput2 = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                                50, 120, 150, 20, hwnd, reinterpret_cast<HMENU>(INPUT2_ID), nullptr, nullptr);

        // ------------------------------------------------------------------------------------

        hText3 = CreateWindowW(L"STATIC", L"UDP Port 1:", WS_VISIBLE | WS_CHILD | ES_LEFT,
                               250, 20, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput3 = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                                250, 50, 150, 20, hwnd, reinterpret_cast<HMENU>(INPUT3_ID), nullptr, nullptr);

        hText4 = CreateWindowW(L"STATIC", L"UDP Port 2:", WS_VISIBLE | WS_CHILD | ES_LEFT,
                               250, 90, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput4 = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                                250, 120, 150, 20, hwnd, reinterpret_cast<HMENU>(INPUT4_ID), nullptr, nullptr);

        // ------------------------------------------------------------------------------------

        hText5 = CreateWindowW(L"STATIC", L"TGS Node ID:", WS_VISIBLE | WS_CHILD | ES_LEFT,
                       50, 160, 150, 20, hwnd, nullptr, nullptr, nullptr);
        hInput5 = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                                50, 190, 150, 20, hwnd, reinterpret_cast<HMENU>(INPUT5_ID), nullptr, nullptr);

        if (foundAddress) {
            std::wstring text = L"Possible Destination IP: " + ethernetAddr + L" (Ethernet IP)";
            ipText = CreateWindowW(L"STATIC", text.c_str(),
                                    WS_CHILD | ES_LEFT,
                                    50, 240, 350, 20, hwnd, nullptr, nullptr, nullptr);
        }

        hButtonTest = CreateWindowW(L"BUTTON", L"Test Connections", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                 150, 290, 150, 30, hwnd, reinterpret_cast<HMENU>(BUTTON1_ID), nullptr, nullptr);


        // Scene 2: all texts as static boxes
        testText1 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 20, 350, 40, hwnd, nullptr, nullptr, nullptr);
        testText2 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 70, 350, 40, hwnd, nullptr, nullptr, nullptr);
        testText3 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 120, 350, 40, hwnd, nullptr, nullptr, nullptr);
        testText4 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 170, 350, 40, hwnd, nullptr, nullptr, nullptr);

        hButton2 = CreateWindowW(L"BUTTON", L"Start Capturing", WS_CHILD | BS_PUSHBUTTON,
                                 175, 220, 100, 30, hwnd, reinterpret_cast<HMENU>(BUTTON2_ID), nullptr, nullptr);

        hButtonStartAnyway = CreateWindowW(L"BUTTON", L"Start Capturing Anyway", WS_CHILD | BS_PUSHBUTTON,
                         125, 260, 200, 30, hwnd, reinterpret_cast<HMENU>(BUTTON3_ID), nullptr, nullptr);

        // Scene 3: text 8 as static box and button
        capturingText = CreateWindowW(L"STATIC", L"Capturing UDP Packets...", WS_CHILD | ES_LEFT,
                                       20, 50, 400, 20, hwnd, nullptr, nullptr, nullptr);

        infoText = CreateWindowW(L"STATIC", full_InfoText.c_str(), WS_CHILD | ES_LEFT,
                                       20, 80, 400, 60, hwnd, nullptr, nullptr, nullptr);

        hButtonStop = CreateWindowW(L"BUTTON", L"Stop Capturing", WS_CHILD | BS_PUSHBUTTON,
                                 150, 200, 100, 30, hwnd, reinterpret_cast<HMENU>(BUTTON4_ID), nullptr, nullptr);

        // Show only scene 1 at start
        ShowScene1(TRUE);
        ShowScene2(FALSE);
        ShowScene3(FALSE);

        break;
    }

    case WM_COMMAND:{
        switch (LOWORD(wParam)) {

            // On "Test Connections" button pressed
            case BUTTON1_ID: {

                // Read inputs on button press
                GetWindowText(GetDlgItem(hwnd, INPUT1_ID), inputSrcIP, sizeof(inputSrcIP));
                GetWindowText(GetDlgItem(hwnd, INPUT2_ID), inputDstIP, sizeof(inputDstIP));
                GetWindowText(GetDlgItem(hwnd, INPUT3_ID), input1, sizeof(input1));
                GetWindowText(GetDlgItem(hwnd, INPUT4_ID), input2, sizeof(input2));
                GetWindowText(GetDlgItem(hwnd, INPUT5_ID), inputTGSNodeID, sizeof(inputTGSNodeID));

                inputStrSrcIP = std::string(inputSrcIP);
                inputStrDstIP = std::string(inputDstIP);
                inputPort1 = atoi(input1);
                inputPort2 = atoi(input2);

                // OutputDebugStringA("First try.\n");

                if (!inputValuesPassTests()){

                    ShowScene1(TRUE);
                    ShowScene2(FALSE);
                    ShowScene3(FALSE);

                    MessageBox(hwnd, "Please enter valid IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::string inputTGSNodeID_str = inputTGSNodeID; // narrow string from input
                TGSNodeID = std::wstring(inputTGSNodeID_str.begin(), inputTGSNodeID_str.end());

                testPorts[0] = inputPort1;
                testPorts[1] = inputPort1 + 1;
                testPorts[2] = inputPort2;
                testPorts[3] = inputPort2 + 1;

                // Run the test first
                testsPassed = false;
                testsPassed = testUdpCommunication(inputDstIP, testPorts, 4);

                ShowScene1(FALSE);
                ShowScene2(TRUE);
                ShowScene3(FALSE);

                if (!testsPassed) {
                    SetWindowText(hButton2, "Go Back");
                    MessageBox(hwnd, "UDP test communication failed. Check your IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }
                SetWindowText(hButton2, "Start Capturing");

                break;
            }

            // On either "Go Back" or "Start Recording" button pressed
            case BUTTON2_ID:
                {
                // If button is in "Go Back" mode:
                if (!testsPassed) {
                    ShowScene1(TRUE);
                    ShowScene2(FALSE);
                    ShowScene3(FALSE);
                    break;
                }

                // Else if button is in "Start Recording" mode:

                // Ask user for output folder
                chosenOutputDirectory = ChooseOutputFolder(hwnd);
                if (chosenOutputDirectory.empty()) {
                    MessageBox(hwnd, "No folder selected. Cancelling.", "Info", MB_OK);
                    break;
                }

                ShowScene1(FALSE);
                ShowScene2(FALSE);
                ShowScene3(TRUE);

                full_InfoText = (
                    L"UDP Source IP: " + std::wstring(inputStrSrcIP.begin(), inputStrSrcIP.end()) +
                    L"\nUDP Destination IP: " + std::wstring(inputStrDstIP.begin(), inputStrDstIP.end()) +
                    L"\nUDP Destination Ports: " +
                    std::to_wstring(inputPort1) + L", " +
                    std::to_wstring(inputPort1 + 1) + L", " +
                    std::to_wstring(inputPort2) + L", " +
                    std::to_wstring(inputPort2 + 1));

                SetWindowTextW(infoText, full_InfoText.c_str());

                GetLocalTime(&sys_time);

                // Initialize listeners with user input
                listeners[0] = { inputDstIP, (inputPort1), inputSrcIP, false, L"ST1_Data"};
                listeners[1] = { inputDstIP, (inputPort1 + 1), inputSrcIP, false, L"ST1_MetaData"};
                listeners[2] = { inputDstIP, (inputPort2), inputSrcIP, false, L"ST2_Data"};
                listeners[3] = { inputDstIP, (inputPort2 + 1), inputSrcIP, false, L"ST2_MetaData"};

                running = true;

                // Start listener threads
                for (int i = 0; i < 4; i++) {
                    threads[i] = std::thread(udpListener, listeners[i], hwnd);
                }
                listenersStarted = true;
                break;

            }

            // On "Start Recording Anyway" button pressed
            case BUTTON3_ID: {
                // At least one test failed but try and record all ports still.

                // Ask user for output folder
                chosenOutputDirectory = ChooseOutputFolder(hwnd);
                if (chosenOutputDirectory.empty()) {
                    MessageBox(hwnd, "No folder selected. Cancelling.", "Info", MB_OK);
                    break;
                }

                ShowScene1(FALSE);
                ShowScene2(FALSE);
                ShowScene3(TRUE);

                full_InfoText = (
                    L"UDP Source IP: " + std::wstring(inputStrSrcIP.begin(), inputStrSrcIP.end()) +
                    L"\nUDP Destination IP: " + std::wstring(inputStrDstIP.begin(), inputStrDstIP.end()) +
                    L"\nUDP Destination Ports: " +
                    std::to_wstring(inputPort1) + L", " +
                    std::to_wstring(inputPort1 + 1) + L", " +
                    std::to_wstring(inputPort2) + L", " +
                    std::to_wstring(inputPort2 + 1));

                SetWindowTextW(infoText, full_InfoText.c_str());

                GetLocalTime(&sys_time);

                // Initialize listeners with user input
                listeners[0] = { inputDstIP, (inputPort1), inputSrcIP,
                    false, L"ST1_Data"};
                listeners[1] = { inputDstIP, (inputPort1 + 1), inputSrcIP,
                    false, L"ST1_MetaData"};
                listeners[2] = { inputDstIP, (inputPort2), inputSrcIP,
                    false, L"ST2_Data"};
                listeners[3] = { inputDstIP, (inputPort2 + 1), inputSrcIP,
                    false, L"ST2_MetaData"};

                running = true;

                // Start listener threads
                for (int i = 0; i < 4; i++) {
                    threads[i] = std::thread(udpListener, listeners[i], hwnd);
                }
                listenersStarted = true;
                break;
            }

            // On "Stop Capturing" button pressed
            case BUTTON4_ID: {
                // Resets the application and turns back to Scene 1, enabling recording multiple times.

                // Stop listeners
                running = false;

                // Join threads
                for (int i = 0; i < 4; i++) {
                    if (threads[i].joinable())
                        threads[i].join();
                }

                listenersStarted = false;

                MessageBox(hwnd, "Listeners stopped.", "Info", MB_OK);

                // Clear input fields
                SetWindowText(hInput1, "");
                SetWindowText(hInput2, "");
                SetWindowText(hInput3, "");
                SetWindowText(hInput4, "");
                SetWindowText(hInput5, "");

                // Reset internal variables
                inputStrDstIP.clear();
                inputStrSrcIP.clear();
                inputPort1 = 0;
                inputPort2 = 0;
                testPorts[0] = testPorts[1] = testPorts[2] = testPorts[3] = 0;
                testsPassed = true;
                chosenOutputDirectory.clear();
                full_InfoText.clear();
                TGSNodeID.clear();

                // Reset test scene text
                SetWindowText(testText1, "Test port");
                SetWindowText(testText2, "Test port");
                SetWindowText(testText3, "Test port");
                SetWindowText(testText4, "Test port");

                // Reset info/capturing scene
                SetWindowText(infoText, "");
                SetWindowText(capturingText, "Capturing UDP Packets...");

                // Show only scene 1
                ShowScene1(TRUE);
                ShowScene2(FALSE);
                ShowScene3(FALSE);
                break;
            }

            default:
                break;
        }
        break;
    }

    case WM_DESTROY:
        {
        // This part executes when the exit button is pressed.
        // Joins the threads and closes the application.
        running = false;

        for (int i = 0; i < 4; i++) {
            if (threads[i].joinable())
                threads[i].join();
        }


        DestroyWindow(hwnd);
        PostQuitMessage(0);
        return 0;
        }

    default:
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{

    // Function that starts the application by creating and showing the window.

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        MessageBox(nullptr, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    const char CLASS_NAME[] = "SceneSwitchWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);

    RegisterClass(&wc);

    ethernetAddr = findEthernetAddress();
    foundAddress = !ethernetAddr.empty();

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "UDP Network Capturer",
        (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)),
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 400,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    WSACleanup();

    return 0;
}
