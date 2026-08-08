# CanBusSimulator

CanBusSimulator is a desktop application built with **C++** and **Qt6** that simulates vehicle CAN Bus signals. It allows users to dynamically load `.dbc` (Database CAN) files and map physical vehicle inputs (Speed, Battery Level, Handbrake, Headlights) into raw hexadecimal CAN payloads. 

This project was developed as a comprehensive software engineering exercise covering GUI design, text parsing, bitwise operations, and networking.

## 🚀 Features

* **Dynamic DBC Parsing:** Uses Regular Expressions (Regex) to parse standard `.dbc` files, extracting `BO_` (Message) and `SG_` (Signal) definitions.
* **Smart Keyword Matching:** Automatically links UI controls to DBC signals using synonym matching (e.g., matching a slider to "Speed", "Spd", or "Hiz").
* **Bitwise Payload Packing:** Converts physical values into raw CAN byte arrays using proper factor/offset math and bitwise shifting (supports Little-Endian formatting).
* **OTA (Over-The-Air) Updates:** Features a built-in self-updating mechanism (`updater.cpp`). It checks a local Dockerized Nginx server for a `version.json` file, downloads the latest `.exe`, and uses a background batch script to seamlessly replace the running executable.
* **Fallback Mode:** Safely falls back to a hardcoded standard CAN frame format (ID: 0x1F4) if no DBC file is provided.

## 🛠️ Technology Stack

* **Language:** C++17
* **Framework:** Qt 6 (Widgets, Network modules)
* **Build System:** CMake & MinGW 64-bit
* **Server Infrastructure:** Docker (Nginx Alpine) for OTA Updates

## 📋 How to Build & Run

### Prerequisites
* [Qt Creator / Qt6](https://www.qt.io/) installed with MinGW toolchain.
* [CMake](https://cmake.org/).
* [Docker](https://www.docker.com/) (Only required for testing the OTA update server).

### Build Instructions
1. Clone the repository.
2. Open the `CMakeLists.txt` file in Qt Creator or VS Code.
3. Build the project using the MinGW 64-bit kit.
4. Run the generated `CanBusSimulator.exe`.

### Testing OTA Updates (Local)
1. Navigate to the `UpdateServer` directory.
2. Build and run the Docker container:
   ```bash
   docker build -t update-server .
   docker run -d -p 8080:80 --name update-server update-server
   ```
3. Click the "Check for Updates" button in the simulator to trigger the self-update process.

## 📝 License
This project is open-source and available under the MIT License.
