#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <string>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <shobjidl.h> // for IFileDialog
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define BUTTON1_ID 101
#define BUTTON2_ID 102
#define BUTTON3_ID 103
#define BUTTON4_ID 104
#define INPUT1_ID 201
#define INPUT2_ID 202
#define INPUT3_ID 203
#define INPUT4_ID 204
#define INPUT5_ID 205

// Scene 1 controls
HWND hText1, hInput1;
HWND hText2, hInput2;
HWND hText3, hInput3;
HWND hText4, hInput4;
HWND hText5, hInput5;
HWND ipText;
HWND hButtonTest;

// Scene 2 controls
HWND test_text_1, test_text_2, test_text_3, test_text_4;
HWND hButtonStartAnyway;
HWND hButton2;

// Scene 3 controls
HWND info_text;
HWND capturing_text;
HWND hButtonStop;

// Shared variables
std::atomic<bool> running(false); // Start as false, start listeners only on button
char input_dst_ip[256], input_src_ip[256], input_1[256], input_2[256], inputTGSNodeID[256];
int input_port_1, input_port_2, testPorts[4];
bool tests_passed = true;
std::string input_dst_ip_str, input_src_ip_str;
std::wstring ethernet_addr, result, full_info_text, TGSNodeID, chosenOutputDirectory;
SYSTEMTIME sys_time;

struct ListenerParams {
    const char* localIP = nullptr;
    int localPort = -1;
    const char* allowedSenderIP = nullptr;
    bool started_recording = false;
    std::wstring fileName;

    ListenerParams() = default;
    ListenerParams(const char* ip, int port, const char* allowedIP, bool recording, const std::wstring& fName)
        : localIP(ip), localPort(port), allowedSenderIP(allowedIP), started_recording(recording), fileName(fName) {}
};

std::thread threads[4];
ListenerParams listeners[4];
bool listenersStarted = false;
bool found_address = false;

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
    ShowWindow(hText5, show);
    ShowWindow(hInput5, show);
    ShowWindow(hButtonTest, show);
    if (found_address) {
        ShowWindow(ipText, show);
    }
}

void ShowScene2(BOOL show)
{
    ShowWindow(test_text_1, show);
    ShowWindow(test_text_2, show);
    ShowWindow(test_text_3, show);
    ShowWindow(test_text_4, show);
    ShowWindow(hButton2, show);
    if (!tests_passed) {
        ShowWindow(hButtonStartAnyway, show);
    }
}

void ShowScene3(BOOL show)
{
    ShowWindow(info_text, show);
    /*
    ShowWindow(test_text_1, show);
    ShowWindow(test_text_2, show);
    ShowWindow(test_text_3, show);
    ShowWindow(test_text_4, show);
    */
    ShowWindow(capturing_text, show);
    ShowWindow(hButtonStop, show);
}

std::wstring find_ethernet_address() {
    ULONG outBufLen = 15000;
    auto adapterAddresses = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(outBufLen));
    if (!adapterAddresses) return L"";

    ULONG retVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &outBufLen);
    if (retVal == ERROR_BUFFER_OVERFLOW) {
        free(adapterAddresses);
        adapterAddresses = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(outBufLen));
        if (!adapterAddresses) return L"";
        retVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &outBufLen);
        // free(adapterAddresses);
    }

    if (retVal == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES adapter = adapterAddresses; adapter; adapter = adapter->Next) {
            if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD &&
                wcscmp(adapter->FriendlyName, L"Ethernet") == 0) {

                for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress;
                     unicast;
                     unicast = unicast->Next) {

                    if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                        auto* ipv4 = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                        std::string ipStr = inet_ntoa(ipv4->sin_addr);
                        result = std::wstring(ipStr.begin(), ipStr.end()); // simple conversion
                        break;
                    }
                }
                }
            if (!result.empty()) break; // Stop once found
        }
    }
    free(adapterAddresses);
    return result; // Empty if not found
}

std::wstring ChooseOutputFolder(HWND hwnd) {
    IFileDialog* pfd = nullptr;
    std::wstring folderPath;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return L"";

    // Create dialog
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                          IID_IFileDialog, reinterpret_cast<void**>(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS); // folder picker mode
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
        pfd->Release();
    }
    CoUninitialize();
    return folderPath;
}

bool input_values_pass_tests() {
    in_addr addr1{};
    in_addr addr2{};

    if (input_src_ip_str.empty() || input_dst_ip_str.empty()
        || input_src_ip_str.find('-') != std::string::npos
        || input_src_ip_str.find('-') != std::string::npos
        || input_src_ip_str == "0.0.0.0" || input_dst_ip_str == "0.0.0.0"
        || input_port_1 > 65535 || input_port_2 > 65535
        || input_port_1 <= 0 || input_port_2 <= 0
        || (inet_pton(AF_INET, input_src_ip, &addr1) != 1)
        || (inet_pton(AF_INET, input_dst_ip, &addr2) != 1)
        || strlen(inputTGSNodeID) == 0
        ) {
            return false;
        }
    return true;
}

void change_test_text(const std::string& input_dst_ip_str, int port_num, int ports[], const std::string& test_message) {
    std::string full_message;
    if (input_dst_ip_str.empty()) {
        full_message = "Test port";
    } else {
        full_message = input_dst_ip_str + ":" + std::to_string(ports[port_num]) + " -> " + test_message;
    }

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

bool testUdpCommunication(const char* localIP, int ports[], int portCount)
{

    for (int i = 0; i < portCount; i++) {
        for (int j = 0; j < portCount; j++) {
            if ((i != j) && (ports[i] == ports[j])) {
                change_test_text(localIP, i, ports,"Ports crashed.");
                change_test_text(localIP, j, ports,"Ports crashed.");
                // OutputDebugStringA("Ports crashed!\n");
                return false;
            }
        }
    }

    // Create sockets for receiving test messages
    SOCKET recvSockets[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };

    for (int i = 0; i < portCount; i++) {
        recvSockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (recvSockets[i] == INVALID_SOCKET) {
            change_test_text(localIP, i, ports,"Failed to create test recv socket.");
            // OutputDebugStringA("Failed to create test recv socket.\n");

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
            change_test_text(localIP, i, ports,"Failed to bind test recv socket 2.");
            // OutputDebugStringA("Failed to bind test recv socket 2.\n");
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
                change_test_text(localIP, i, ports,"Failed with timeout.");
            }
            break;
        }
        if (selectRes == SOCKET_ERROR) {
            for (int i = 0; i < portCount; i++) {
                change_test_text(localIP, i, ports,"select() failed during test.");
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
                    change_test_text(localIP, i, ports,"Tests successful.");
                    remaining--;
                } else {
                    change_test_text(localIP, i, ports,"RecvLen < 0.");
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
    std::ofstream MyFile;
    char buffer[1600];
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    // Keep trying until we can start recording
    while (!params.started_recording && running) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            // OutputDebugStringA("Bind failed!\n");
            continue;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(params.localIP);
        serverAddr.sin_port = htons(params.localPort);

        BOOL optVal = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optVal), sizeof(optVal));

        if (bind(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            // OutputDebugStringA("Bind failed!\n");
            continue;  // Retry
        }

        // example params.fileName = "ST1_MetaData"
        // example fullFileName = "ST1_MetaData_TGSNodeID_YearMonthDay_HourMinut.bin"
        std::wstring fullFileName = (chosenOutputDirectory + L"\\" + params.fileName + L"_" +
            TGSNodeID + L"_" +
            std::to_wstring(sys_time.wYear) + L"_" +
            std::to_wstring(sys_time.wMonth) + L"_" +
            std::to_wstring(sys_time.wDay) + L"_" +
            std::to_wstring(sys_time.wHour) + L"_" +
            std::to_wstring(sys_time.wMinute) +
            L".bin");

        MyFile.open(fullFileName.c_str(), std::ios::binary);
        if (!MyFile.is_open()) {
            closesocket(sock);
            // OutputDebugStringA("Bind failed!\n");
            continue;  // Retry
        }

        params.started_recording = true;  // Success! start recording
        // OutputDebugStringA("Starting the record.\n");
    }

    // Recording loop
    while (running && params.started_recording) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeval timeout{};
        timeout.tv_sec = 1;  // check running every second
        timeout.tv_usec = 0;

        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);
        if (selectResult > 0 && FD_ISSET(sock, &readfds)) {
            int recvLen = recvfrom(sock, buffer, sizeof(buffer), 0,
                                   reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

            if (recvLen == SOCKET_ERROR) {
                std::cout << "recvFrom failed" << std::endl;
                break;
            }

            std::string senderIP = inet_ntoa(clientAddr.sin_addr);
            if (senderIP == params.allowedSenderIP) {

                MyFile.write(buffer, recvLen);
                MyFile.flush();
            }
        }
    }

    if (sock != INVALID_SOCKET) closesocket(sock);
    if (MyFile.is_open()) MyFile.close();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
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

        if (found_address) {
            std::wstring text = L"Possible Destination IP: " + ethernet_addr + L" (Ethernet IP)";
            ipText = CreateWindowW(L"STATIC", text.c_str(),
                                    WS_CHILD | ES_LEFT,
                                    50, 240, 350, 20, hwnd, nullptr, nullptr, nullptr);
        }

        hButtonTest = CreateWindowW(L"BUTTON", L"Test Connections", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                 150, 290, 150, 30, hwnd, reinterpret_cast<HMENU>(BUTTON1_ID), nullptr, nullptr);


        // Scene 2: all texts as static boxes
        test_text_1 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 20, 350, 40, hwnd, nullptr, nullptr, nullptr);
        test_text_2 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 70, 350, 40, hwnd, nullptr, nullptr, nullptr);
        test_text_3 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 120, 350, 40, hwnd, nullptr, nullptr, nullptr);
        test_text_4 = CreateWindowW(L"STATIC", L"Test port", WS_CHILD | ES_LEFT,
                                    50, 170, 350, 40, hwnd, nullptr, nullptr, nullptr);

        hButton2 = CreateWindowW(L"BUTTON", L"Start Capturing", WS_CHILD | BS_PUSHBUTTON,
                                 175, 220, 100, 30, hwnd, reinterpret_cast<HMENU>(BUTTON2_ID), nullptr, nullptr);

        hButtonStartAnyway = CreateWindowW(L"BUTTON", L"Start Capturing Anyway", WS_CHILD | BS_PUSHBUTTON,
                         125, 260, 200, 30, hwnd, reinterpret_cast<HMENU>(BUTTON3_ID), nullptr, nullptr);

        // Scene 3: text 8 as static box and button
        capturing_text = CreateWindowW(L"STATIC", L"Capturing UDP Packets...", WS_CHILD | ES_LEFT,
                                       20, 50, 400, 20, hwnd, nullptr, nullptr, nullptr);

        info_text = CreateWindowW(L"STATIC", full_info_text.c_str(), WS_CHILD | ES_LEFT,
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
                GetWindowText(GetDlgItem(hwnd, INPUT1_ID), input_src_ip, sizeof(input_src_ip));
                GetWindowText(GetDlgItem(hwnd, INPUT2_ID), input_dst_ip, sizeof(input_dst_ip));
                GetWindowText(GetDlgItem(hwnd, INPUT3_ID), input_1, sizeof(input_1));
                GetWindowText(GetDlgItem(hwnd, INPUT4_ID), input_2, sizeof(input_2));
                GetWindowText(GetDlgItem(hwnd, INPUT5_ID), inputTGSNodeID, sizeof(inputTGSNodeID));

                input_src_ip_str = std::string(input_src_ip);
                input_dst_ip_str = std::string(input_dst_ip);
                input_port_1 = atoi(input_1);
                input_port_2 = atoi(input_2);

                // OutputDebugStringA("First try.\n");

                if (!input_values_pass_tests()){

                    ShowScene1(TRUE);
                    ShowScene2(FALSE);
                    ShowScene3(FALSE);

                    MessageBox(hwnd, "Please enter valid IP and ports.", "Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::string inputTGSNodeID_str = inputTGSNodeID; // narrow string from input
                TGSNodeID = std::wstring(inputTGSNodeID_str.begin(), inputTGSNodeID_str.end());

                testPorts[0] = input_port_1;
                testPorts[1] = input_port_1 + 1;
                testPorts[2] = input_port_2;
                testPorts[3] = input_port_2 + 1;

                // Run the test first
                tests_passed = false;
                tests_passed = testUdpCommunication(input_dst_ip, testPorts, 4);

                ShowScene1(FALSE);
                ShowScene2(TRUE);
                ShowScene3(FALSE);

                if (!tests_passed) {
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
                // If "Go Back"
                if (!tests_passed) {
                    ShowScene1(TRUE);
                    ShowScene2(FALSE);
                    ShowScene3(FALSE);
                    break;
                }

                // Else if "Start Recording"

                // Ask user for output folder
                chosenOutputDirectory = ChooseOutputFolder(hwnd);
                if (chosenOutputDirectory.empty()) {
                    MessageBox(hwnd, "No folder selected. Cancelling.", "Info", MB_OK);
                    break;
                }

                ShowScene1(FALSE);
                ShowScene2(FALSE);
                ShowScene3(TRUE);

                full_info_text = (
                    L"UDP Source IP: " + std::wstring(input_src_ip_str.begin(), input_src_ip_str.end()) +
                    L"\nUDP Destination IP: " + std::wstring(input_dst_ip_str.begin(), input_dst_ip_str.end()) +
                    L"\nUDP Destination Ports: " +
                    std::to_wstring(input_port_1) + L", " +
                    std::to_wstring(input_port_1 + 1) + L", " +
                    std::to_wstring(input_port_2) + L", " +
                    std::to_wstring(input_port_2 + 1));

                SetWindowTextW(info_text, full_info_text.c_str());

                GetLocalTime(&sys_time);

                // Initialize listeners with user input
                listeners[0] = { input_dst_ip, (input_port_1), input_src_ip, false, L"ST1_Data"};
                listeners[1] = { input_dst_ip, (input_port_1 + 1), input_src_ip, false, L"ST1_MetaData"};
                listeners[2] = { input_dst_ip, (input_port_2), input_src_ip, false, L"ST2_Data"};
                listeners[3] = { input_dst_ip, (input_port_2 + 1), input_src_ip, false, L"ST2_MetaData"};

                running = true;

                // Start listener threads
                for (int i = 0; i < 4; i++) {
                    threads[i] = std::thread(udpListener, listeners[i], hwnd);
                }
                listenersStarted = true;
                break;

            }

            // On "Start Recording Anyway" button pressed
            case BUTTON3_ID: { // At least one test failed but try and record all ports still.

                // Ask user for output folder
                chosenOutputDirectory = ChooseOutputFolder(hwnd);
                if (chosenOutputDirectory.empty()) {
                    MessageBox(hwnd, "No folder selected. Cancelling.", "Info", MB_OK);
                    break;
                }

                ShowScene1(FALSE);
                ShowScene2(FALSE);
                ShowScene3(TRUE);

                full_info_text = (
                    L"UDP Source IP: " + std::wstring(input_src_ip_str.begin(), input_src_ip_str.end()) +
                    L"\nUDP Destination IP: " + std::wstring(input_dst_ip_str.begin(), input_dst_ip_str.end()) +
                    L"\nUDP Destination Ports: " +
                    std::to_wstring(input_port_1) + L", " +
                    std::to_wstring(input_port_1 + 1) + L", " +
                    std::to_wstring(input_port_2) + L", " +
                    std::to_wstring(input_port_2 + 1));

                SetWindowTextW(info_text, full_info_text.c_str());

                GetLocalTime(&sys_time);

                // Initialize listeners with user input
                listeners[0] = { input_dst_ip, (input_port_1), input_src_ip,
                    false, L"ST1_Data"};
                listeners[1] = { input_dst_ip, (input_port_1 + 1), input_src_ip,
                    false, L"ST1_MetaData"};
                listeners[2] = { input_dst_ip, (input_port_2), input_src_ip,
                    false, L"ST2_Data"};
                listeners[3] = { input_dst_ip, (input_port_2 + 1), input_src_ip,
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

            default:
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

    default:
        break;
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

    const char CLASS_NAME[] = "SceneSwitchWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(240, 248, 255));

    RegisterClass(&wc);

    ethernet_addr = find_ethernet_address();
    found_address = !ethernet_addr.empty();

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Telemetry Data Recorder",
        (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)),
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 400,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
        return 0;



    /*
    ShowScene1(TRUE);
    ShowScene2(FALSE);
    ShowScene3(FALSE);
    */


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
