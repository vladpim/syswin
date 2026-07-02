<!-- markdownlint-disable MD033 -->
<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/Windows-Vista%2B-brightgreen.svg" alt="Windows"/>
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="MIT License"/>
  <img src="https://img.shields.io/badge/header--only-yes-orange.svg" alt="Header-only"/>
  <img src="https://img.shields.io/badge/version-1.1%20Stable-red.svg" alt="Version"/>
</p>

# 🪟 syswin.hpp – Windows System Information Library

**Single‑header, modern C++ library to query every corner of Windows**  
*No external dependencies, no DLLs – just `#include` and go.*

```cpp
#include "syswin.hpp"
std::string cpu = syswin::get_cpu_name();   // "..."

📋 Features at a glance
Category	What you can get
💻 Hardware	CPU name, physical/logical cores, GPU name, total RAM, disk info (size, free space, file system)
📈 Live telemetry	CPU usage (delta), RAM used/free (GB), current process memory
⚙️ Processes	List all running processes (PID, name, path, memory), kill a process by PID
🚀 Startup	Read/write/delete HKCU/HKLM Run entries with environment variable expansion
🧠 OS & System	Windows version (real), build number, uptime, computer name, architecture, system directories
👑 Administrator	Check admin rights, check elevation level, relaunch as admin (UAC)
📦 Installed SW	Enumerate 32/64‑bit installed applications from registry (name, version, publisher, uninstall)
👤 Current user	Username, domain, SID
🌐 Network	List adapters (name, MAC, speed, status), get local IPv4/IPv6, get MAC address
🔋 Battery	Percentage, charging state, remaining minutes (laptops)
🛠️ Windows Services	List all services with status (Running/Stopped) and startup type (Auto/Manual/Disabled)
🎤 Audio devices	Count and details of output/input devices (WinMM)
🌍 Environment	Get all variables or a specific one
🔌 Power management	Shutdown, restart, sleep, hibernate (new in v1.1)
🆕 What's New in v1.1
Power management – added shutdown_system(), restart_system(), sleep_system(), hibernate_system().

Fixed get_startup_commands() – now dynamically allocates buffer for registry values, avoiding truncation of long commands.

Fixed get_running_processes() – explicitly initializes memory_mb to 0, eliminating garbage values for system processes.

Improved RAII consistency – replaced manual CloseHandle with UniqueHandle in process enumeration.

Added missing header <powrprof.h> and library powrprof.lib for power functions.

Enhanced documentation – added detailed comments and usage notes throughout the code.

📦 Installation
Just copy syswin.hpp into your project and include it.
That’s it – no build steps, no extra libraries. All necessary .lib files are automatically linked via #pragma comment.

bash
git clone https://github.com/VladPim/syswin.git
cd your_project
cp syswin/syswin.hpp .
🚀 Quick Start
cpp
#include <iostream>
#include "syswin.hpp"

int main() {
    // Basic hardware
    std::cout << "CPU  : " << syswin::get_cpu_name() << "\n";
    std::cout << "GPU  : " << syswin::get_gpu_name() << "\n";
    std::cout << "RAM  : " << syswin::get_total_ram_gb() << " GB total\n";

    // Live CPU usage (need two calls)
    syswin::get_cpu_usage();           // first call – baseline
    Sleep(1000);
    unsigned cpu = syswin::get_cpu_usage();
    std::cout << "CPU usage: " << cpu << "%\n";

    // Network
    std::wcout << L"Your IP : " << syswin::get_local_ipv4() << L"\n";

    // Shutdown system (use with care!)
    // syswin::shutdown_system(false);
    return 0;
}
📚 Detailed Examples
1️⃣ CPU & Hardware
cpp
auto cores = syswin::get_cpu_cores();
std::cout << "Physical cores: " << cores.physical_cores
          << ", Logical cores: " << cores.logical_cores << "\n";

for (const auto& disk : syswin::get_disks_info()) {
    std::wcout << L"Drive " << disk.drive_letter
               << L" – " << disk.total_bytes / (1024*1024*1024) << L" GB total, "
               << disk.free_bytes / (1024*1024*1024) << L" GB free\n";
}
2️⃣ Processes & Memory
cpp
for (const auto& proc : syswin::get_running_processes()) {
    std::wcout << L"PID " << proc.pid
               << L" : " << proc.name
               << L" [" << proc.memory_mb << L" MB]\n";
}

// Kill a process (use with care)
syswin::terminate_process(12345);
3️⃣ Startup commands (autostart)
cpp
// Add current user startup
syswin::add_startup_current_user(L"MyApp", L"C:\\tools\\myapp.exe --quiet");

// Read all
auto cmds = syswin::get_startup_commands();
for (auto& cmd : cmds) std::wcout << cmd << L"\n";

// Remove
syswin::remove_startup_current_user(L"MyApp");
4️⃣ Administrator elevation
cpp
if (!syswin::is_admin()) {
    if (syswin::run_as_admin()) {
        return 0;   // new elevated process launched, exit this one
    } else {
        std::cerr << "User cancelled UAC prompt.\n";
    }
}
// Here we are elevated
5️⃣ Network adapters
cpp
std::wcout << L"IPv4: " << syswin::get_local_ipv4() << L"\n";
std::wcout << L"MAC : " << syswin::get_mac_address() << L"\n";

for (const auto& a : syswin::get_network_adapters()) {
    std::wcout << L"Adapter: " << a.name
               << L", Speed: " << a.speed_mbps << L" Mbps"
               << L", MAC: " << a.mac_address << L"\n";
}
6️⃣ Installed software
cpp
auto sw = syswin::get_installed_software();
for (const auto& prog : sw) {
    std::wcout << prog.name << L" v" << prog.version
               << L" by " << prog.publisher << L"\n";
}
7️⃣ Battery status (laptop only)
cpp
auto bat = syswin::get_battery_status();
if (bat.is_battery_present) {
    std::cout << "Battery: " << bat.percentage << "%";
    if (bat.is_charging) std::cout << " (charging)";
    if (bat.remaining_minutes >= 0)
        std::cout << ", " << bat.remaining_minutes << " min left";
    std::cout << "\n";
}
8️⃣ Windows services
cpp
auto services = syswin::get_services();
for (const auto& svc : services) {
    std::wcout << svc.name << L" – " << svc.status
               << L" (start: " << svc.start_type << L")\n";
}
9️⃣ Environment variables
cpp
std::wstring path = syswin::get_env_var(L"PATH");
auto all = syswin::get_all_env_vars();
for (auto& [name, value] : all) {
    // do something
}
🔟 Power management (new in v1.1)
cpp
// Restart system after 5 seconds (force true)
if (syswin::restart_system(true)) {
    std::cout << "System is restarting...\n";
}

// Put system to sleep
if (syswin::sleep_system()) {
    std::cout << "System going to sleep.\n";
}
⚠️ Important Notes
Note	Description
CPU usage	First call returns 0 (baseline). Subsequent calls give usage since last call. Thread‑safe.
Winsock	Network functions automatically initialise/cleanup Winsock with reference counting. No manual WSAStartup.
Registry	HKLM uninstall keys are read with both 32‑ and 64‑bit views. Startup write needs admin rights for HKLM.
Process memory	Some protected processes may return 0 MB due to insufficient privileges.
Strings	Most functions return std::wstring (UTF‑16). Use syswin::to_utf8() / syswin::to_wstring() to convert.
Thread safety	All functions are thread‑safe unless noted (get_cpu_usage() uses a mutex).
Elevation	run_as_admin() does not exit the current process – you should do that yourself after the call.
Power functions	Require SE_SHUTDOWN_NAME privilege, typically held by administrators.
🔧 Requirements & Compatibility
OS: Windows Vista / 7 / 8 / 10 / 11 (x86, x64, ARM64)

Compiler: Any C++17 compiler (MSVC 2019+, Clang-cl, MinGW-w64 11+).

No external dependencies – pure Win32 API.

The library automatically links:

psapi.lib

advapi32.lib

iphlpapi.lib

winmm.lib

ws2_32.lib

shell32.lib

powrprof.lib (new in v1.1)

📖 API Reference
All functions are inside namespace syswin and are inline.

Hardware
std::string get_cpu_name()

CpuCoreInfo get_cpu_cores()

std::string get_gpu_name()

unsigned long long get_total_ram_gb()

std::vector<DiskInfo> get_disks_info()

Telemetry
unsigned get_cpu_usage()

RamUsage get_ram_usage()

unsigned long long get_current_process_memory_mb()

Processes & Startup
std::vector<Process> get_running_processes()

bool terminate_process(DWORD pid)

std::vector<std::wstring> get_startup_commands()

bool add_startup_current_user(...), remove_..., add_startup_local_machine, remove_...

OS & System
std::wstring get_windows_version(), DWORD get_windows_build_number()

unsigned long long get_system_uptime_seconds()

std::wstring get_computer_name(), get_system_directory(), get_windows_directory()

std::string get_os_architecture()

Admin rights
bool is_admin(), bool is_process_elevated(), bool run_as_admin(const std::wstring& parameters = L"")

Installed software
std::vector<InstalledProgram> get_installed_software()

Current user
std::wstring get_current_username(), get_current_user_domain(), get_current_user_sid()

Network
std::vector<NetworkAdapter> get_network_adapters()

std::wstring get_local_ipv4(), get_local_ipv6(), get_mac_address()

Battery
BatteryStatus get_battery_status()

Services
std::vector<ServiceInfo> get_services()

Audio
unsigned int get_audio_output_devices_count(), get_audio_input_devices_count()

std::vector<AudioDeviceInfo> get_audio_output_devices(), get_audio_input_devices()

Environment
std::vector<std::pair<std::wstring, std::wstring>> get_all_env_vars()

std::wstring get_env_var(const std::wstring& name)

Power management (new)
bool shutdown_system(bool force)

bool restart_system(bool force)

bool sleep_system()

bool hibernate_system()

Utilities
std::string to_utf8(const std::wstring&)

std::wstring to_wstring(const std::string&)
