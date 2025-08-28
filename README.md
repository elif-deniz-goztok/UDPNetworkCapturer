---

# UDP Network Capturer

A zero external dependency **Windows-based UDP Network Capturer** built in C++11 that captures UDP packets from specific source and destination IPs and ports, with a user-friendly GUI for configuration and recording. 

With the capability of writing multiple MBs under a second, this project can work with high-speed UDP networks. It is ideal for high traffic network monitoring scenarios.

---

## Features

* **UDP packet capture** for up to 4 ports simultaneously.
* **Dual-port listening** – when you enter ports (e.g., 5555 and 6666), the application also listens on their immediate next ports (5556 and 6667). This allows capturing paired UDP streams commonly used in network protocols.
* **Graphical user interface (GUI)** with 3 scenes:

  1. **Configuration Scene** – enter source/destination IPs and ports.
  3. **Test Scene** – test UDP connectivity and port availability before recording.
  4. **Recording Scene** – capture UDP packets and save them to a chosen folder.
* **Buffered file writing** for efficient storage of high-volume UDP traffic.
* **Threaded listeners** to record multiple streams concurrently.
* **Automatic Ethernet IP detection** for quick destination IP suggestions.
* **Input validation** to prevent invalid IPs or ports.

---

## Screenshots

Home screen:

<img width="417" height="372" alt="Ekran görüntüsü 2025-08-28 093913" src="https://github.com/user-attachments/assets/e717901a-3970-4c6c-83a2-45bfe66cb1dc" />

<img width="417" height="372" alt="Ekran görüntüsü 2025-08-28 093927" src="https://github.com/user-attachments/assets/e108643a-bf3f-4d2d-88d6-ad9601e1d6cf" />

Test results for each port:

<img width="417" height="372" alt="Ekran görüntüsü 2025-08-28 094603" src="https://github.com/user-attachments/assets/3a7ff0c5-a51d-48df-bec2-a94e888769f3" />

<img width="417" height="372" alt="Ekran görüntüsü 2025-08-28 094033" src="https://github.com/user-attachments/assets/a3f7a081-86ab-43d0-9262-9da7ccb0420d" />

While capturing:

<img width="417" height="372" alt="Ekran görüntüsü 2025-08-28 094120" src="https://github.com/user-attachments/assets/a1b44294-008e-4ab3-8443-2626972ac719" />


---

## Prerequisites

* Windows 10/11
* Visual Studio or any C++ compiler supporting C++11
* Windows SDK
* Winsock2 library (`ws2_32.lib`)
* IP Helper API (`iphlpapi.lib`)

---

## Build & Run

You can either download the latest release or build from source.

# Option 1 — Download Release (Recommended)

Head over to the Releases page and grab the latest prebuilt executable.
No setup needed — just download and run!

# Option 2 — Build from Source

If you prefer to build it yourself:

1 - Clone the repository
  git clone https://github.com/your-username/your-repo.git
  cd your-repo

2 - Create a build directory
  mkdir build && cd build

3 - Run CMake and build
  cmake ..
  cmake --build . --config Release


The compiled executable will be located in:

build/Release/


Run it from there, or move it somewhere more convenient.

## Usage

1. **Scene 1 – Configuration**:

   * Enter the **UDP source IP**, **destination IP** and **ports**.
     > Note: For each port you enter, the application also checks and records traffic on the next sequential port (e.g., entering 5555 will also include 5556).
   * Click **Test Connections** to validate inputs and check port availability.

2. **Scene 2 – Test**:

   * The application performs UDP port tests and displays results.
   * If tests fail, you can either **Go Back** to correct inputs or **Start Capturing Anyway**.

3. **Scene 3 – Recording**:

   * Choose an output folder for storing captured files.
   * The application starts recording UDP packets in binary format, buffering for efficiency.
   * Click **Stop Capturing** to end the recording session.

---

## Output Files

* Files are stored in the selected folder with the naming convention:

```
<PortName>_YYYY_MM_DD_HH_MM.bin
```

Example:

```
Port1_2025_08_27_16_30.bin
```

---

## Code Structure

* `WinMain` – Initializes WinSock, creates the main window, and runs the message loop.
* `WindowProc` – Handles GUI events and scene switching.
* `udpListener` – Threaded function to capture UDP packets and write to binary files.
* `testUdpCommunication` – Tests UDP connectivity and port collisions.
* `findEthernetAddress` – Detects the Ethernet IP address of the host machine.
* `ChooseOutputFolder` – Opens a folder picker dialog for selecting output location.
* GUI scenes managed via `ShowScene1`, `ShowScene2`, `ShowScene3`.

---

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/YourFeature`.
3. Commit your changes: `git commit -m "Add your feature"`.
4. Push to the branch: `git push origin feature/YourFeature`.
5. Open a pull request.

---

## License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

---
