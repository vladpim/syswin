#include "syswin.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== System Hardware Info ===\n\n";

    // 1. CPU
    std::cout << "[1] CPU...\n";
    try {
        std::cout << "  Name: " << syswin::get_cpu_name() << "\n";
        auto cores = syswin::get_cpu_cores();
        std::cout << "  Cores: " << cores.physical_cores << " physical, " << cores.logical_cores << " logical\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get CPU info\n";
    }

    // 2. RAM
    std::cout << "\n[2] RAM...\n";
    try {
        std::cout << "  Total: " << syswin::get_total_ram_gb() << " GB\n";
        auto ram = syswin::get_ram_usage();
        std::cout << "  Used: " << ram.used_gb << " GB, Free: " << ram.free_gb << " GB\n";
        std::cout << "  Current process memory: " << syswin::get_current_process_memory_mb() << " MB\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get RAM info\n";
    }

    // 3. GPU
    std::cout << "\n[3] GPU...\n";
    try {
        std::cout << "  Name: " << syswin::get_gpu_name() << "\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get GPU info\n";
    }

    // 4. Drives
    std::cout << "\n[4] Drives...\n";
    try {
        auto disks = syswin::get_disks_info();
        for (const auto& d : disks) {
            std::cout << "  " << syswin::to_utf8(d.drive_letter)
                      << "  " << syswin::to_utf8(d.filesystem)
                      << "  Total: " << d.total_bytes / (1024ULL*1024*1024) << " GB"
                      << "  Free: " << d.free_bytes / (1024ULL*1024*1024) << " GB\n";
        }
    } catch (...) {
        std::cout << "  [ERROR] Failed to get drives info\n";
    }

    // 5. Network adapters
    std::cout << "\n[5] Network adapters...\n";
    try {
        auto adapters = syswin::get_network_adapters();
        std::cout << "  Count: " << adapters.size() << "\n";
        for (const auto& a : adapters) {
            std::cout << "    " << syswin::to_utf8(a.name)
                      << ", MAC: " << syswin::to_utf8(a.mac_address)
                      << ", Speed: " << a.speed_mbps << " Mbps\n";
        }
        std::cout << "  MAC address (first active): " << syswin::to_utf8(syswin::get_mac_address()) << "\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get network adapters\n";
    }

    // 6. IP addresses
    std::cout << "\n[6] IP addresses...\n";
    try {
        std::wstring ip4 = syswin::get_local_ipv4();
        std::wstring ip6 = syswin::get_local_ipv6();
        std::cout << "  IPv4: " << syswin::to_utf8(ip4) << "\n";
        std::cout << "  IPv6: " << syswin::to_utf8(ip6) << "\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get IP addresses\n";
    }

    // 7. Battery
    std::cout << "\n[7] Battery...\n";
    try {
        auto bat = syswin::get_battery_status();
        if (bat.is_battery_present)
            std::cout << "  Charge: " << bat.percentage << "%\n";
        else
            std::cout << "  No battery\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get battery info\n";
    }

    // 8. OS & System info
    std::cout << "\n[8] OS & System info...\n";
    try {
        std::cout << "  Version: " << syswin::to_utf8(syswin::get_windows_version()) << "\n";
        std::cout << "  Build number: " << syswin::get_windows_build_number() << "\n";
        std::cout << "  Architecture: " << syswin::get_os_architecture() << "\n";
        std::cout << "  Computer name: " << syswin::to_utf8(syswin::get_computer_name()) << "\n";
        std::cout << "  Username: " << syswin::to_utf8(syswin::get_current_username()) << "\n";
        std::cout << "  Domain: " << syswin::to_utf8(syswin::get_current_user_domain()) << "\n";
        std::cout << "  User SID: " << syswin::to_utf8(syswin::get_current_user_sid()) << "\n";
        std::cout << "  System directory: " << syswin::to_utf8(syswin::get_system_directory()) << "\n";
        std::cout << "  Windows directory: " << syswin::to_utf8(syswin::get_windows_directory()) << "\n";
        std::cout << "  Uptime: " << syswin::get_system_uptime_seconds()/3600 << " hours\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get OS info\n";
    }

    // 9. Processes (top 10 by memory)
    std::cout << "\n[9] Running processes (top 10 by memory)...\n";
    try {
        auto procs = syswin::get_running_processes();
        std::sort(procs.begin(), procs.end(), [](const syswin::Process& a, const syswin::Process& b) {
            return a.memory_mb > b.memory_mb;
        });
        int count = 0;
        for (const auto& p : procs) {
            if (count++ >= 10) break;
            std::cout << "    PID " << p.pid << "  " << syswin::to_utf8(p.name)
                      << "  " << p.memory_mb << " MB\n";
        }
    } catch (...) {
        std::cout << "  [ERROR] Failed to get processes\n";
    }

    // 10. Startup commands
    std::cout << "\n[10] Startup commands...\n";
    try {
        auto startup = syswin::get_startup_commands();
        std::cout << "  Count: " << startup.size() << "\n";
        for (const auto& cmd : startup) {
            std::cout << "    " << syswin::to_utf8(cmd) << "\n";
        }
    } catch (...) {
        std::cout << "  [ERROR] Failed to get startup commands\n";
    }

    // 11. Installed software (first 20)
    std::cout << "\n[11] Installed software (first 20)...\n";
    try {
        auto sw = syswin::get_installed_software();
        std::cout << "  Total entries: " << sw.size() << "\n";
        int count = 0;
        for (const auto& prog : sw) {
            if (count++ >= 20) break;
            std::cout << "    " << syswin::to_utf8(prog.name)
                      << "  v" << syswin::to_utf8(prog.version)
                      << "  (" << syswin::to_utf8(prog.publisher) << ")\n";
        }
    } catch (...) {
        std::cout << "  [ERROR] Failed to get installed software\n";
    }

    // 12. Services (first 20)
    std::cout << "\n[12] Windows services (first 20)...\n";
    try {
        auto services = syswin::get_services();
        std::cout << "  Total: " << services.size() << "\n";
        int count = 0;
        for (const auto& s : services) {
            if (count++ >= 20) break;
            std::cout << "    " << syswin::to_utf8(s.name)
                      << "  [" << syswin::to_utf8(s.status) << "]"
                      << "  start: " << syswin::to_utf8(s.start_type) << "\n";
        }
    } catch (...) {
        std::cout << "  [ERROR] Failed to get services\n";
    }

    // 13. Audio devices
    std::cout << "\n[13] Audio devices...\n";
    try {
        std::cout << "  Output devices: " << syswin::get_audio_output_devices_count() << "\n";
        auto outDevs = syswin::get_audio_output_devices();
        for (const auto& d : outDevs) {
            std::cout << "    " << syswin::to_utf8(d.product_name)
                      << "  (channels: " << d.channels << ")\n";
        }
        std::cout << "  Input devices: " << syswin::get_audio_input_devices_count() << "\n";
        auto inDevs = syswin::get_audio_input_devices();
        for (const auto& d : inDevs) {
            std::cout << "    " << syswin::to_utf8(d.product_name)
                      << "  (channels: " << d.channels << ")\n";
        }
    } catch (...) {
        std::cout << "  [ERROR] Failed to get audio devices\n";
    }

    // 14. Environment variables (first 15)
    std::cout << "\n[14] Environment variables (first 15)...\n";
    try {
        auto env = syswin::get_all_env_vars();
        std::cout << "  Total: " << env.size() << "\n";
        int count = 0;
        for (const auto& e : env) {
            if (count++ >= 15) break;
            std::cout << "    " << syswin::to_utf8(e.first) << " = " << syswin::to_utf8(e.second) << "\n";
        }
        std::cout << "  PATH = " << syswin::to_utf8(syswin::get_env_var(L"PATH")) << "\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get environment variables\n";
    }

    // 15. Admin & elevation
    std::cout << "\n[15] Security...\n";
    try {
        std::cout << "  Is admin: " << (syswin::is_admin() ? "Yes" : "No") << "\n";
        std::cout << "  Process elevated: " << (syswin::is_process_elevated() ? "Yes" : "No") << "\n";
        // run_as_admin не вызываем автоматически, только показываем, что функция существует
        std::cout << "  (run_as_admin() available)\n";
    } catch (...) {
        std::cout << "  [ERROR] Failed to get security info\n";
    }

    std::cout << "\nDone. Press Enter to exit...";
    std::cin.get();
    return 0;
}