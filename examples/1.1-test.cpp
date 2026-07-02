// main.cpp – тест syswin.hpp v1.1
#include "syswin.hpp"
#include <iostream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <windows.h>

// Вспомогательная функция для вывода широких строк в консоль (UTF-8)
static void print_wstring(const std::wstring& ws) {
    if (ws.empty()) {
        std::cout << "(empty)";
        return;
    }
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    std::cout << conv.to_bytes(ws);
}

template<typename T>
static void print_vector(const std::vector<T>& vec, const std::string& label) {
    std::cout << label << " (" << vec.size() << "):\n";
    for (const auto& item : vec) {
        std::cout << "  ";
        if constexpr (std::is_same_v<T, syswin::DiskInfo>) {
            std::cout << "Drive: "; print_wstring(item.drive_letter);
            std::cout << ", Total: " << item.total_bytes / (1024ULL*1024*1024) << " GB";
            std::cout << ", Free: " << item.free_bytes / (1024ULL*1024*1024) << " GB";
            std::cout << ", FS: "; print_wstring(item.filesystem);
        } else if constexpr (std::is_same_v<T, syswin::Process>) {
            std::cout << "PID=" << item.pid << ", Name="; print_wstring(item.name);
            std::cout << ", Mem=" << item.memory_mb << " MB";
        } else if constexpr (std::is_same_v<T, syswin::InstalledProgram>) {
            std::cout << "Name="; print_wstring(item.name);
            std::cout << ", Version="; print_wstring(item.version);
            std::cout << ", Publisher="; print_wstring(item.publisher);
        } else if constexpr (std::is_same_v<T, syswin::ServiceInfo>) {
            std::cout << "Name="; print_wstring(item.name);
            std::cout << ", Display="; print_wstring(item.display_name);
            std::cout << ", Status="; print_wstring(item.status);
            std::cout << ", Start="; print_wstring(item.start_type);
        } else if constexpr (std::is_same_v<T, syswin::NetworkAdapter>) {
            std::cout << "Name="; print_wstring(item.name);
            std::cout << ", MAC="; print_wstring(item.mac_address);
            std::cout << ", Speed=" << item.speed_mbps << " Mbps";
            std::cout << ", Up=" << (item.is_up ? "Yes" : "No");
        } else if constexpr (std::is_same_v<T, syswin::AudioDeviceInfo>) {
            std::cout << "ID=" << item.device_id << ", Name="; print_wstring(item.product_name);
            std::cout << ", Channels=" << item.channels;
        } else {
            // fallback: пытаемся вывести через operator<< если есть
            std::cout << " [unsupported type]";
        }
        std::cout << "\n";
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "=== syswin.hpp v1.1 Test ===\n\n";

    // 1. Hardware info
    std::cout << "CPU Name: " << syswin::get_cpu_name() << "\n";
    auto cores = syswin::get_cpu_cores();
    std::cout << "Physical cores: " << cores.physical_cores << ", Logical cores: " << cores.logical_cores << "\n";
    std::cout << "GPU: " << syswin::get_gpu_name() << "\n";
    std::cout << "Total RAM: " << syswin::get_total_ram_gb() << " GB\n";

    // 2. Disks
    auto disks = syswin::get_disks_info();
    print_vector(disks, "Disks");

    // 3. Live telemetry
    std::cout << "CPU Usage: " << syswin::get_cpu_usage() << "%\n";
    auto ram = syswin::get_ram_usage();
    std::cout << "RAM Used: " << ram.used_gb << " GB, Free: " << ram.free_gb << " GB\n";
    std::cout << "Current process memory: " << syswin::get_current_process_memory_mb() << " MB\n";

    // 4. Processes (first 5 only)
    auto procs = syswin::get_running_processes();
    if (procs.size() > 5) procs.resize(5);
    print_vector(procs, "Processes (first 5)");

    // 5. Startup commands
    auto startup = syswin::get_startup_commands();
    std::cout << "Startup commands (" << startup.size() << "):\n";
    for (const auto& cmd : startup) {
        std::cout << "  "; print_wstring(cmd); std::cout << "\n";
    }

    // 6. Administrator
    std::cout << "Is Admin: " << (syswin::is_admin() ? "Yes" : "No") << "\n";
    std::cout << "Is Elevated: " << (syswin::is_process_elevated() ? "Yes" : "No") << "\n";

    // 7. OS info
    std::cout << "Windows version: "; print_wstring(syswin::get_windows_version()); std::cout << "\n";
    std::cout << "Build number: " << syswin::get_windows_build_number() << "\n";
    std::cout << "Uptime: " << syswin::get_system_uptime_seconds() << " sec\n";
    std::cout << "Computer name: "; print_wstring(syswin::get_computer_name()); std::cout << "\n";
    std::cout << "OS arch: " << syswin::get_os_architecture() << "\n";
    std::cout << "System dir: "; print_wstring(syswin::get_system_directory()); std::cout << "\n";
    std::cout << "Windows dir: "; print_wstring(syswin::get_windows_directory()); std::cout << "\n";

    // 8. Installed software (first 5)
    auto programs = syswin::get_installed_software();
    if (programs.size() > 5) programs.resize(5);
    print_vector(programs, "Installed programs (first 5)");

    // 9. User info
    std::cout << "Username: "; print_wstring(syswin::get_current_username()); std::cout << "\n";
    std::cout << "User domain: "; print_wstring(syswin::get_current_user_domain()); std::cout << "\n";
    std::cout << "User SID: "; print_wstring(syswin::get_current_user_sid()); std::cout << "\n";

    // 10. Network adapters
    auto adapters = syswin::get_network_adapters();
    print_vector(adapters, "Network adapters");

    std::cout << "Local IPv4: "; print_wstring(syswin::get_local_ipv4()); std::cout << "\n";
    std::cout << "Local IPv6: "; print_wstring(syswin::get_local_ipv6()); std::cout << "\n";
    std::cout << "MAC Address: "; print_wstring(syswin::get_mac_address()); std::cout << "\n";

    // 11. Environment variables (first 5)
    auto envs = syswin::get_all_env_vars();
    std::cout << "Environment variables (first 5):\n";
    int count = 0;
    for (const auto& [name, value] : envs) {
        if (++count > 5) break;
        std::cout << "  "; print_wstring(name); std::cout << "="; print_wstring(value); std::cout << "\n";
    }

    // 12. Services (first 5)
    auto services = syswin::get_services();
    if (services.size() > 5) services.resize(5);
    print_vector(services, "Services (first 5)");

    // 13. Audio devices
    std::cout << "Audio output devices count: " << syswin::get_audio_output_devices_count() << "\n";
    auto outDevs = syswin::get_audio_output_devices();
    print_vector(outDevs, "Audio output devices");
    auto inDevs = syswin::get_audio_input_devices();
    print_vector(inDevs, "Audio input devices");

    // 14. Battery
    auto batt = syswin::get_battery_status();
    if (batt.is_battery_present) {
        std::cout << "Battery: " << batt.percentage << "%";
        if (batt.is_charging) std::cout << " (charging)";
        else if (batt.is_discharging) std::cout << " (discharging)";
        if (batt.remaining_minutes >= 0)
            std::cout << ", remaining " << batt.remaining_minutes << " min";
        std::cout << "\n";
    } else {
        std::cout << "Battery: not present\n";
    }

    // 15. Power management (функции только выводят, но не выполняют, чтобы случайно не выключить систему)
    std::cout << "\nPower management functions available (not called):\n";
    std::cout << "  shutdown_system(), restart_system(), hibernate_system(), sleep_system()\n";

    // 16. Пример работы с Run as admin (не вызываем, только проверяем существование)
    std::cout << "\nNote: run_as_admin() is available but not invoked.\n";

    std::cout << "\n=== Test completed successfully ===\n";

    // Ждём нажатия клавиши, чтобы консоль не закрылась
    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}