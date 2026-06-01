// syswin.hpp
// Version: 1.0 Stable
// Repository: [https://github.com/VladPim/syswin]
// License: MIT
//
// A header-only library to retrieve system information on Windows.
// All functions are inside the namespace 'syswin' and are inline.
//
// This library provides a clean, RAII-based interface to query:
//   - Hardware info (CPU, RAM, disks, GPU)
//   - Live telemetry (CPU usage, RAM usage)
//   - Process list, startup commands, service list
//   - Network adapters, IP addresses, MAC
//   - Battery status, audio devices, environment variables
//   - Windows version, uptime, administrator rights
//   - Installed software from registry
//   - And much more.
//
// All functions are thread-safe where necessary, and all handles are
// automatically managed via custom deleters.

#pragma once

// Force UNICODE before including any Windows headers
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WINVER
#define WINVER 0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

// NOTE: winsock2.h must be included before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <sddl.h>
#include <winternl.h>
#include <mmsystem.h>
#include <shellapi.h>

#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <algorithm>
#include <bitset>
#include <mutex>
#include <atomic>
#include <system_error>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

namespace syswin {

    // =========================================================================
    // RAII wrappers for system handles
    // =========================================================================

    /**
     * @brief Custom deleter for HANDLE types (files, processes, threads, snapshots, etc.)
     *
     * Closes the handle only if it is valid and not INVALID_HANDLE_VALUE.
     * Used with std::unique_ptr to provide automatic handle management.
     */
    struct HandleDeleter {
        void operator()(HANDLE h) const noexcept {
            if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
        }
    };

    /// Alias for a unique handle that auto-closes on destruction.
    using UniqueHandle = std::unique_ptr<void, HandleDeleter>;

    /**
     * @brief Helper function to create a UniqueHandle from a Windows API function that returns HANDLE.
     *
     * If the function returns INVALID_HANDLE_VALUE, the returned UniqueHandle will be empty.
     *
     * @tparam Func Type of the function (usually deduced)
     * @tparam Args Types of arguments
     * @param func The function to call (e.g., CreateToolhelp32Snapshot)
     * @param args Arguments to pass to the function
     * @return UniqueHandle that owns the handle, or nullptr if creation failed.
     *
     * @example
     * auto snap = make_unique_handle(CreateToolhelp32Snapshot, TH32CS_SNAPPROCESS, 0);
     */
    template<typename Func, typename... Args>
    inline UniqueHandle make_unique_handle(Func&& func, Args&&... args) {
        HANDLE h = func(std::forward<Args>(args)...);
        if (h == INVALID_HANDLE_VALUE) return UniqueHandle(nullptr);
        return UniqueHandle(h);
    }

    /**
     * @brief Custom deleter for registry keys (HKEY).
     */
    struct RegKeyDeleter {
        void operator()(HKEY h) const noexcept { if (h) RegCloseKey(h); }
    };

    /// Alias for a unique registry key that auto-closes.
    using UniqueRegKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegKeyDeleter>;

    /**
     * @brief Opens a registry key and returns a RAII wrapper.
     *
     * @param root    Root key (e.g., HKEY_LOCAL_MACHINE).
     * @param subkey  Subkey path (e.g., L"SOFTWARE\\...")
     * @param access  Desired access rights (e.g., KEY_READ).
     * @return UniqueRegKey owning the opened key, or nullptr on failure.
     */
    inline UniqueRegKey make_unique_regkey(HKEY root, const wchar_t* subkey, REGSAM access) {
        HKEY h = nullptr;
        if (RegOpenKeyExW(root, subkey, 0, access, &h) != ERROR_SUCCESS)
            return UniqueRegKey(nullptr);
        return UniqueRegKey(h);
    }

    /**
     * @brief RAII wrapper for a SOCKET (Winsock).
     *
     * Automatically calls closesocket() on destruction.
     * Non-copyable, movable.
     */
    class UniqueSocket {
    private:
        SOCKET sock = INVALID_SOCKET;
    public:
        UniqueSocket(SOCKET s = INVALID_SOCKET) noexcept : sock(s) {}
        ~UniqueSocket() { reset(); }

        UniqueSocket(const UniqueSocket&) = delete;
        UniqueSocket& operator=(const UniqueSocket&) = delete;

        UniqueSocket(UniqueSocket&& other) noexcept : sock(other.sock) {
            other.sock = INVALID_SOCKET;
        }

        UniqueSocket& operator=(UniqueSocket&& other) noexcept {
            if (this != &other) {
                reset();
                sock = other.sock;
                other.sock = INVALID_SOCKET;
            }
            return *this;
        }

        void reset(SOCKET s = INVALID_SOCKET) noexcept {
            if (sock != INVALID_SOCKET) closesocket(sock);
            sock = s;
        }

        SOCKET get() const noexcept { return sock; }
        explicit operator bool() const noexcept { return sock != INVALID_SOCKET; }
    };

    /**
     * @brief Deleter for service control manager handles (SC_HANDLE).
     */
    struct ScHandleDeleter {
        void operator()(SC_HANDLE h) const noexcept { if (h) CloseServiceHandle(h); }
    };

    /// Alias for a unique service handle.
    using UniqueScHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ScHandleDeleter>;

    /**
     * @brief Deleter that calls LocalFree().
     */
    struct LocalFreeDeleter {
        void operator()(void* p) const noexcept { if (p) LocalFree(p); }
    };

    /// Alias for a unique pointer to memory allocated by LocalAlloc or similar.
    template<typename T>
    using UniqueLocalPtr = std::unique_ptr<T, LocalFreeDeleter>;

    /**
     * @brief Deleter for environment string blocks returned by GetEnvironmentStringsW().
     */
    struct EnvStringsDeleter {
        void operator()(LPWCH p) const noexcept { if (p) FreeEnvironmentStringsW(p); }
    };

    /// Alias for a unique environment block.
    using UniqueEnvStrings = std::unique_ptr<wchar_t[], EnvStringsDeleter>;

    // =========================================================================
    // Winsock initialization (thread-safe)
    // =========================================================================
    namespace detail {
        /**
         * @brief Manages Winsock startup/shutdown with reference counting.
         *
         * Thread-safe: multiple concurrent calls to acquire() will only call WSAStartup once.
         * The manager is a singleton.
         */
        class WinsockManager {
        private:
            std::atomic<int> ref_count{0};
            std::atomic<bool> init_failed{false};
            std::mutex mtx;
        public:
            WinsockManager() = default;

            /**
             * @brief Increments the reference count and initializes Winsock if needed.
             * @return true if Winsock is ready, false on failure.
             */
            bool acquire() noexcept {
                std::lock_guard<std::mutex> lock(mtx);
                if (init_failed) return false;
                if (ref_count == 0) {
                    WSADATA wsaData;
                    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                        init_failed = true;
                        return false;
                    }
                }
                ref_count++;
                return true;
            }

            /**
             * @brief Decrements the reference count and calls WSACleanup when it reaches zero.
             */
            void release() noexcept {
                std::lock_guard<std::mutex> lock(mtx);
                if (init_failed || ref_count == 0) return;
                if (--ref_count == 0) {
                    WSACleanup();
                }
            }

            bool is_initialized() const noexcept {
                return ref_count > 0 && !init_failed;
            }
        };

        /// Returns the global Winsock manager instance.
        inline WinsockManager& get_winsock_manager() {
            static WinsockManager manager;
            return manager;
        }

        /**
         * @brief RAII guard that acquires Winsock on construction and releases on destruction.
         *
         * All functions that need Winsock should create a local instance of this guard.
         * Example:
         *   detail::WinsockGuard ws;
         *   if (!ws) return {}; // Winsock unavailable
         */
        class WinsockGuard {
        private:
            bool ok;
        public:
            WinsockGuard() noexcept : ok(get_winsock_manager().acquire()) {}
            ~WinsockGuard() { if (ok) get_winsock_manager().release(); }
            explicit operator bool() const noexcept { return ok; }
            WinsockGuard(const WinsockGuard&) = delete;
            WinsockGuard& operator=(const WinsockGuard&) = delete;
        };
    }

    // =========================================================================
    // String conversion utilities
    // =========================================================================

    /**
     * @brief Converts a UTF-16 wide string to a UTF-8 narrow string.
     * @param wstr Input wide string (UTF-16).
     * @return UTF-8 encoded std::string, or empty string on failure.
     */
    [[nodiscard]] inline std::string to_utf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                       nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(size, 0);
        int converted = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                             result.data(), size, nullptr, nullptr);
        if (converted != size) return {};
        return result;
    }

    /**
     * @brief Converts a UTF-8 narrow string to a UTF-16 wide string.
     * @param str Input UTF-8 string.
     * @return UTF-16 std::wstring, or empty string on failure.
     */
    [[nodiscard]] inline std::wstring to_wstring(const std::string& str) {
        if (str.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                                       nullptr, 0);
        if (size <= 0) return {};
        std::wstring result(size, 0);
        int converted = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                                             result.data(), size);
        if (converted != size) return {};
        return result;
    }

    // =========================================================================
    // 1. Static hardware information
    // =========================================================================

    /**
     * @brief Retrieves the CPU name as reported by the registry.
     * @return CPU name string (e.g., "Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz").
     *         Returns "Unknown" on failure.
     */
    [[nodiscard]] inline std::string get_cpu_name() {
        auto key = make_unique_regkey(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            KEY_READ | KEY_WOW64_64KEY);
        if (!key) return "Unknown";

        DWORD size = 0;
        if (RegQueryValueExW(key.get(), L"ProcessorNameString", nullptr, nullptr, nullptr, &size) != ERROR_SUCCESS)
            return "Unknown";
        if (size == 0) return "Unknown";

        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 2, L'\0');
        DWORD actual_size = size;
        if (RegQueryValueExW(key.get(), L"ProcessorNameString", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buffer.data()), &actual_size) != ERROR_SUCCESS)
            return "Unknown";
        return to_utf8(buffer.data());
    }

    /**
     * @brief Contains physical and logical CPU core counts.
     */
    struct CpuCoreInfo {
        unsigned physical_cores = 0;   ///< Number of physical cores (e.g., 4 for a quad-core)
        unsigned logical_cores = 0;    ///< Number of logical cores (including Hyper-Threading)
    };

    /**
     * @brief Determines the number of physical and logical CPU cores.
     * @return CpuCoreInfo structure with the core counts. On error, both fields are 0.
     *
     * This function uses GetLogicalProcessorInformation and handles dynamic buffer resizing.
     */
    [[nodiscard]] inline CpuCoreInfo get_cpu_cores() noexcept {
        CpuCoreInfo info;
        DWORD size = 0;
        GetLogicalProcessorInformation(nullptr, &size);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
            return info;

        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer;
        while (true) {
            buffer.resize(size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) + 2);
            if (GetLogicalProcessorInformation(buffer.data(), &size))
                break;
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                continue;
            return info;
        }

        for (const auto& item : buffer) {
            if (item.Relationship == RelationProcessorCore) {
                info.physical_cores++;
                info.logical_cores += static_cast<unsigned>(std::bitset<64>(item.ProcessorMask).count());
            }
        }
        return info;
    }

    /**
     * @brief Returns the name of the primary graphics adapter.
     * @return GPU name as std::string, e.g., "NVIDIA GeForce RTX 2080", or "Unknown".
     */
    [[nodiscard]] inline std::string get_gpu_name() {
        DISPLAY_DEVICEW dd;
        dd.cb = sizeof(dd);
        for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
            if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                return to_utf8(dd.DeviceString);
            }
        }
        return "Unknown";
    }

    /**
     * @brief Gets total installed RAM in gigabytes (rounded down).
     * @return Total physical memory in GB.
     */
    [[nodiscard]] inline unsigned long long get_total_ram_gb() noexcept {
        MEMORYSTATUSEX ms = {};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        return ms.ullTotalPhys / (1024ULL * 1024 * 1024);
    }

    /**
     * @brief Information about a logical disk drive.
     */
    struct DiskInfo {
        std::wstring drive_letter;      ///< Drive letter with trailing backslash, e.g., L"C:\\"
        unsigned long long total_bytes; ///< Total size of the drive in bytes
        unsigned long long free_bytes;  ///< Free space on the drive in bytes
        std::wstring filesystem;        ///< File system name (e.g., L"NTFS", L"FAT32")
    };

    /**
     * @brief Enumerates all logical drives and retrieves their size, free space, and file system.
     * @return std::vector<DiskInfo> containing information for each drive.
     */
    [[nodiscard]] inline std::vector<DiskInfo> get_disks_info() {
        std::vector<DiskInfo> result;
        wchar_t drives[256];
        if (GetLogicalDriveStringsW(sizeof(drives)/sizeof(wchar_t), drives) == 0)
            return result;

        for (wchar_t* d = drives; *d; d += wcslen(d) + 1) {
            DiskInfo info;
            info.drive_letter = d;

            ULARGE_INTEGER free_bytes_available, total_bytes, total_free;
            if (GetDiskFreeSpaceExW(d, &free_bytes_available, &total_bytes, &total_free)) {
                info.total_bytes = total_bytes.QuadPart;
                info.free_bytes = total_free.QuadPart;
            }

            wchar_t fs_name[MAX_PATH + 1] = {};
            if (GetVolumeInformationW(d, nullptr, 0, nullptr, nullptr, nullptr, fs_name, MAX_PATH))
                info.filesystem = fs_name;
            else
                info.filesystem = L"Unknown";

            result.push_back(std::move(info));
        }
        return result;
    }

    // =========================================================================
    // 2. Live telemetry (CPU usage, RAM usage)
    // =========================================================================
    namespace detail {
        /**
         * @brief Internal state for CPU usage calculation.
         * Stores previous idle/kernel/user times and a mutex for thread safety.
         */
        struct CpuUsageState {
            std::mutex mtx;
            FILETIME prev_idle = {};
            FILETIME prev_kernel = {};
            FILETIME prev_user = {};
            bool first_call = true;
        };
        inline CpuUsageState& get_cpu_usage_state() {
            static CpuUsageState state;
            return state;
        }
    }

    /**
     * @brief Computes the overall CPU usage percentage since the last call.
     *
     * The first call returns 0 because no delta is available.
     * Subsequent calls return the percentage of non-idle time between calls.
     *
     * @return CPU usage as an integer from 0 to 100.
     *
     * @note This function is thread-safe.
     */
    [[nodiscard]] inline unsigned get_cpu_usage() {
        auto& state = detail::get_cpu_usage_state();
        std::lock_guard<std::mutex> lock(state.mtx);

        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user))
            return 0;

        if (state.first_call) {
            state.prev_idle = idle;
            state.prev_kernel = kernel;
            state.prev_user = user;
            state.first_call = false;
            return 0;
        }

        ULARGE_INTEGER cur_idle   = { { idle.dwLowDateTime,   idle.dwHighDateTime } };
        ULARGE_INTEGER cur_kernel = { { kernel.dwLowDateTime, kernel.dwHighDateTime } };
        ULARGE_INTEGER cur_user   = { { user.dwLowDateTime,   user.dwHighDateTime } };
        ULARGE_INTEGER prev_idle  = { { state.prev_idle.dwLowDateTime,   state.prev_idle.dwHighDateTime } };
        ULARGE_INTEGER prev_kernel= { { state.prev_kernel.dwLowDateTime, state.prev_kernel.dwHighDateTime } };
        ULARGE_INTEGER prev_user  = { { state.prev_user.dwLowDateTime,   state.prev_user.dwHighDateTime } };

        ULONGLONG delta_idle   = cur_idle.QuadPart   - prev_idle.QuadPart;
        ULONGLONG delta_kernel = cur_kernel.QuadPart - prev_kernel.QuadPart;
        ULONGLONG delta_user   = cur_user.QuadPart   - prev_user.QuadPart;
        ULONGLONG delta_total  = delta_kernel + delta_user;

        state.prev_idle   = idle;
        state.prev_kernel = kernel;
        state.prev_user   = user;

        if (delta_total == 0) return 0;
        return static_cast<unsigned>(100 - (delta_idle * 100 / delta_total));
    }

    /**
     * @brief RAM usage in gigabytes (used and free).
     */
    struct RamUsage {
        unsigned long long used_gb = 0; ///< Used physical memory in GB.
        unsigned long long free_gb = 0; ///< Available physical memory in GB.
    };

    /**
     * @brief Returns current RAM usage (used and free) in gigabytes.
     * @return RamUsage structure.
     */
    [[nodiscard]] inline RamUsage get_ram_usage() noexcept {
        MEMORYSTATUSEX ms = {};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        RamUsage ru;
        ru.used_gb = (ms.ullTotalPhys - ms.ullAvailPhys) / (1024ULL * 1024 * 1024);
        ru.free_gb = ms.ullAvailPhys / (1024ULL * 1024 * 1024);
        return ru;
    }

    /**
     * @brief Retrieves the current process's working set size (physical memory) in megabytes.
     * @return Memory in MB, or 0 on failure.
     */
    [[nodiscard]] inline unsigned long long get_current_process_memory_mb() noexcept {
        HANDLE hProc = GetCurrentProcess();
        PROCESS_MEMORY_COUNTERS pmc = {};
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc)))
            return pmc.WorkingSetSize / (1024ULL * 1024);
        return 0;
    }

    // =========================================================================
    // 3. Process management and startup commands
    // =========================================================================

    /**
     * @brief Information about a running process.
     */
    struct Process {
        DWORD pid;                   ///< Process ID.
        std::wstring name;          ///< Executable name (e.g., L"notepad.exe").
        std::wstring path;          ///< Full path to the executable (if accessible).
        unsigned long long memory_mb; ///< Working set size in megabytes.
    };

    /**
     * @brief Enumerates all running processes and retrieves name, path, and memory usage.
     * @return std::vector<Process> containing information for each process.
     *
     * @note Path retrieval uses PROCESS_QUERY_LIMITED_INFORMATION; memory usage requires
     *       additional privileges and may fail for some processes.
     */
    [[nodiscard]] inline std::vector<Process> get_running_processes() {
        std::vector<Process> result;
        auto snap = make_unique_handle(CreateToolhelp32Snapshot, TH32CS_SNAPPROCESS, 0);
        if (!snap) return result;

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);
        if (!Process32FirstW(snap.get(), &pe))
            return result;

        do {
            Process proc;
            proc.pid = pe.th32ProcessID;
            proc.name = pe.szExeFile;

            auto hProc = make_unique_handle(OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc.pid);
            if (hProc) {
                wchar_t path[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc.get(), 0, path, &size))
                    proc.path = path;

                // Memory info requires higher privileges
                HANDLE hMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc.pid);
                if (hMem) {
                    PROCESS_MEMORY_COUNTERS pmc = {};
                    if (GetProcessMemoryInfo(hMem, &pmc, sizeof(pmc)))
                        proc.memory_mb = pmc.WorkingSetSize / (1024ULL * 1024);
                    CloseHandle(hMem);
                }
            }
            result.push_back(std::move(proc));
        } while (Process32NextW(snap.get(), &pe));

        return result;
    }

    /**
     * @brief Expands environment variables in a string (e.g., %USERPROFILE%).
     * @param src Input wide string possibly containing %VAR% placeholders.
     * @return Expanded string, or the original if expansion fails.
     */
    static std::wstring expand_env_string(const std::wstring& src) {
        DWORD size = ExpandEnvironmentStringsW(src.c_str(), nullptr, 0);
        if (size == 0) return src;
        std::vector<wchar_t> buffer(size);
        if (ExpandEnvironmentStringsW(src.c_str(), buffer.data(), size) == 0)
            return src;
        return std::wstring(buffer.data());
    }

    /**
     * @brief Retrieves the list of commands that run at system startup.
     *
     * Reads from both HKCU and HKLM Run registry keys. REG_EXPAND_SZ values are expanded.
     *
     * @return std::vector<std::wstring> containing each command line.
     */
    [[nodiscard]] inline std::vector<std::wstring> get_startup_commands() {
        std::vector<std::wstring> commands;
        const std::pair<HKEY, std::wstring> roots[] = {
            {HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"},
            {HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"}
        };
        for (const auto& [root, path] : roots) {
            REGSAM access = KEY_READ;
            if (root == HKEY_LOCAL_MACHINE) access |= KEY_WOW64_64KEY;
            HKEY hKey;
            if (RegOpenKeyExW(root, path.c_str(), 0, access, &hKey) != ERROR_SUCCESS)
                continue;
            DWORD index = 0;
            wchar_t valueName[256];
            BYTE data[1024];
            DWORD nameSize, dataSize, type;
            while (true) {
                nameSize = 256;
                dataSize = sizeof(data);
                LONG res = RegEnumValueW(hKey, index, valueName, &nameSize,
                    nullptr, &type, data, &dataSize);
                if (res == ERROR_NO_MORE_ITEMS) break;
                if (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
                    std::wstring command((wchar_t*)data, dataSize / sizeof(wchar_t));
                    while (!command.empty() && command.back() == L'\0') command.pop_back();
                    if (type == REG_EXPAND_SZ) command = expand_env_string(command);
                    commands.push_back(command);
                }
                ++index;
            }
            RegCloseKey(hKey);
        }
        return commands;
    }

    /**
     * @brief Adds a startup entry for the current user (HKCU).
     * @param name    Name of the value (appears in Task Manager Startup tab).
     * @param command Command line to run (can include environment variables).
     * @return true if successful, false otherwise.
     */
    inline bool add_startup_current_user(const std::wstring& name, const std::wstring& command) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) return false;
        LONG res = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ,
                                  (const BYTE*)command.c_str(),
                                  (DWORD)((command.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    /**
     * @brief Removes a startup entry for the current user (HKCU).
     * @param name Name of the value to delete.
     * @return true if successful, false otherwise.
     */
    inline bool remove_startup_current_user(const std::wstring& name) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) return false;
        LONG res = RegDeleteValueW(hKey, name.c_str());
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    /**
     * @brief Adds a startup entry for all users (HKLM). Requires administrator privileges.
     * @param name    Name of the value.
     * @param command Command line to run.
     * @return true if successful, false otherwise.
     */
    inline bool add_startup_local_machine(const std::wstring& name, const std::wstring& command) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) return false;
        LONG res = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ,
                                  (const BYTE*)command.c_str(),
                                  (DWORD)((command.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    /**
     * @brief Removes a startup entry for all users (HKLM). Requires administrator privileges.
     * @param name Name of the value to delete.
     * @return true if successful, false otherwise.
     */
    inline bool remove_startup_local_machine(const std::wstring& name) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) return false;
        LONG res = RegDeleteValueW(hKey, name.c_str());
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    // =========================================================================
    // 4. Administrator rights and elevation
    // =========================================================================

    /**
     * @brief Checks if the current user is a member of the Administrators group.
     * @return true if the user has administrator privileges, false otherwise.
     *
     * @note This checks the token's group membership, not the integrity level.
     *       For elevation check, use is_process_elevated().
     */
    [[nodiscard]] inline bool is_admin() noexcept {
        BOOL is_member = FALSE;
        PSID admins_group = nullptr;
        SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins_group)) {
            CheckTokenMembership(nullptr, admins_group, &is_member);
            FreeSid(admins_group);
        }
        return is_member != FALSE;
    }

    /**
     * @brief Attempts to relaunch the current process with administrator rights.
     *
     * If already running as admin, returns true immediately without relaunching.
     * Otherwise, triggers a UAC prompt using ShellExecuteEx with "runas".
     *
     * @param parameters Optional command-line parameters to pass to the new instance.
     * @return true if the process was already admin or the elevation request succeeded,
     *         false if elevation failed or was cancelled by the user.
     *
     * @note The current process does not terminate automatically; you should exit
     *       after calling this if elevation is required.
     */
    [[nodiscard]] inline bool run_as_admin(const std::wstring& parameters = L"") {
        if (is_admin()) return true;
        wchar_t exe_path[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH))
            return false;

        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = exe_path;
        sei.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
        sei.nShow = SW_NORMAL;
        return ShellExecuteExW(&sei) != FALSE;
    }

    // =========================================================================
    // 5. OS and system information
    // =========================================================================

    /**
     * @brief Retrieves a human-readable Windows version string.
     * @return std::wstring like L"Windows 10.0.19045 (Build 19045)" or L"Windows 11 ..."
     *
     * Uses RtlGetVersion (which is not affected by application manifests) to get
     * the true OS version.
     */
    [[nodiscard]] inline std::wstring get_windows_version() {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return L"Unknown";

        using RtlGetVersionPtr = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
        auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (!RtlGetVersion) return L"Unknown";

        RTL_OSVERSIONINFOW ver = {};
        ver.dwOSVersionInfoSize = sizeof(ver);
        if (RtlGetVersion(&ver) != 0) return L"Unknown";

        wchar_t buffer[128];
        if (ver.dwMajorVersion == 10 && ver.dwBuildNumber >= 22000)
            _snwprintf_s(buffer, _TRUNCATE, L"Windows 11 %lu.%lu (Build %lu)", ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber);
        else
            _snwprintf_s(buffer, _TRUNCATE, L"Windows %lu.%lu (Build %lu)", ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber);
        return std::wstring(buffer);
    }

    /**
     * @brief Returns the Windows build number (e.g., 19045).
     * @return DWORD build number, or 0 on failure.
     */
    [[nodiscard]] inline DWORD get_windows_build_number() noexcept {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return 0;

        using RtlGetVersionPtr = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
        auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (!RtlGetVersion) return 0;

        RTL_OSVERSIONINFOW ver = {};
        ver.dwOSVersionInfoSize = sizeof(ver);
        if (RtlGetVersion(&ver) != 0) return 0;
        return ver.dwBuildNumber;
    }

    /**
     * @brief Returns system uptime in seconds.
     * @return Uptime in seconds (from GetTickCount64).
     */
    [[nodiscard]] inline unsigned long long get_system_uptime_seconds() noexcept {
        return GetTickCount64() / 1000;
    }

    /**
     * @brief Returns the computer's NetBIOS name.
     * @return Computer name as std::wstring, or L"Unknown" on failure.
     */
    [[nodiscard]] inline std::wstring get_computer_name() noexcept {
        wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(buffer) / sizeof(wchar_t);
        if (GetComputerNameW(buffer, &size))
            return std::wstring(buffer);
        return L"Unknown";
    }

    /**
     * @brief Returns the operating system architecture (x86, x64, ARM64).
     * @return std::string like "x64", "x86", "ARM64", or "Unknown".
     */
    [[nodiscard]] inline std::string get_os_architecture() noexcept {
        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        default: return "Unknown";
        }
    }

    /**
     * @brief Returns the system directory path (e.g., C:\Windows\System32).
     * @return Wide string path.
     */
    [[nodiscard]] inline std::wstring get_system_directory() noexcept {
        wchar_t buffer[MAX_PATH];
        if (GetSystemDirectoryW(buffer, MAX_PATH))
            return std::wstring(buffer);
        return L"";
    }

    /**
     * @brief Returns the Windows directory path (e.g., C:\Windows).
     * @return Wide string path.
     */
    [[nodiscard]] inline std::wstring get_windows_directory() noexcept {
        wchar_t buffer[MAX_PATH];
        if (GetWindowsDirectoryW(buffer, MAX_PATH))
            return std::wstring(buffer);
        return L"";
    }

    // =========================================================================
    // 6. Installed programs (from registry Uninstall keys)
    // =========================================================================

    /**
     * @brief Information about an installed application.
     */
    struct InstalledProgram {
        std::wstring name;             ///< Display name of the program.
        std::wstring version;          ///< Version string (may be empty).
        std::wstring publisher;        ///< Publisher name (may be empty).
        std::wstring install_date;     ///< Install date as stored in registry (may be empty or in different formats).
        std::wstring uninstall_string; ///< Command line used to uninstall the program.
    };

    /**
     * @brief Reads a string value from a registry key, expanding REG_EXPAND_SZ if needed.
     * @param hKey      Open registry key.
     * @param valueName Name of the value to read.
     * @return The string value, or empty string if not found.
     */
    static std::wstring read_reg_string(HKEY hKey, const wchar_t* valueName) {
        DWORD type = 0;
        DWORD size = 0;
        LONG ret = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &size);
        if (ret != ERROR_SUCCESS) return L"";
        if (type != REG_SZ && type != REG_EXPAND_SZ) return L"";
        if (size == 0) return L"";

        std::vector<BYTE> buffer(size + sizeof(wchar_t));
        DWORD actualSize = size + sizeof(wchar_t);
        ret = RegQueryValueExW(hKey, valueName, nullptr, &type, buffer.data(), &actualSize);
        if (ret != ERROR_SUCCESS) return L"";
        if (actualSize == 0) return L"";

        std::wstring result(reinterpret_cast<wchar_t*>(buffer.data()), actualSize / sizeof(wchar_t));
        while (!result.empty() && result.back() == L'\0') result.pop_back();
        if (type == REG_EXPAND_SZ) result = expand_env_string(result);
        return result;
    }

    /**
     * @brief Enumerates installed software from the registry (both 32-bit and 64-bit views).
     *
     * Reads from:
     *   - HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall
     *   - HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall
     *
     * @return std::vector<InstalledProgram> containing all found applications with DisplayName.
     */
    [[nodiscard]] inline std::vector<InstalledProgram> get_installed_software() {
        std::vector<InstalledProgram> result;
        const wchar_t* roots[] = {
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
        };

        for (int i = 0; i < 2; ++i) {
            REGSAM sam = KEY_READ | (i == 0 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY);
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, roots[i], 0, sam, &hKey) != ERROR_SUCCESS)
                continue;
            UniqueRegKey key(hKey);

            DWORD index = 0;
            wchar_t subKeyName[256];
            while (true) {
                DWORD nameLen = 256;
                LONG res = RegEnumKeyExW(key.get(), index, subKeyName, &nameLen, nullptr, nullptr, nullptr, nullptr);
                if (res == ERROR_NO_MORE_ITEMS) break;
                if (res != ERROR_SUCCESS) {
                    ++index;
                    continue;
                }
                HKEY hSub;
                if (RegOpenKeyExW(key.get(), subKeyName, 0, sam, &hSub) == ERROR_SUCCESS) {
                    UniqueRegKey sub(hSub);
                    std::wstring name = read_reg_string(sub.get(), L"DisplayName");
                    if (!name.empty()) {
                        InstalledProgram prog;
                        prog.name = name;
                        prog.version = read_reg_string(sub.get(), L"DisplayVersion");
                        prog.publisher = read_reg_string(sub.get(), L"Publisher");
                        prog.install_date = read_reg_string(sub.get(), L"InstallDate");
                        prog.uninstall_string = read_reg_string(sub.get(), L"UninstallString");
                        result.push_back(std::move(prog));
                    }
                }
                ++index;
            }
        }
        return result;
    }

    // =========================================================================
    // 7. Current user information
    // =========================================================================

    /**
     * @brief Returns the username of the currently logged-on user.
     * @return Wide string containing the username, or empty on failure.
     */
    [[nodiscard]] inline std::wstring get_current_username() noexcept {
        wchar_t buffer[256];
        DWORD size = sizeof(buffer) / sizeof(wchar_t);
        if (GetUserNameW(buffer, &size))
            return std::wstring(buffer);
        return L"";
    }

    /**
     * @brief Returns the domain name of the current user.
     * @return Domain name as std::wstring, or the computer name if the user is local.
     */
    [[nodiscard]] inline std::wstring get_current_user_domain() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            return get_computer_name();
        UniqueHandle token(hToken);

        DWORD tokenInfoSize = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenInfoSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return get_computer_name();

        std::vector<BYTE> buffer(tokenInfoSize);
        PTOKEN_USER tokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
        if (!GetTokenInformation(hToken, TokenUser, tokenUser, tokenInfoSize, &tokenInfoSize))
            return get_computer_name();

        wchar_t domainName[256];
        wchar_t userName[256];
        DWORD domainSize = sizeof(domainName) / sizeof(wchar_t);
        DWORD userNameSize = sizeof(userName) / sizeof(wchar_t);
        SID_NAME_USE sidType;

        if (LookupAccountSidW(nullptr, tokenUser->User.Sid, userName, &userNameSize, domainName, &domainSize, &sidType)) {
            return std::wstring(domainName);
        }
        return get_computer_name();
    }

    /**
     * @brief Returns the Security Identifier (SID) of the current user as a string.
     * @return SID string (e.g., L"S-1-5-21-..."), or empty on failure.
     */
    [[nodiscard]] inline std::wstring get_current_user_sid() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            return L"";
        UniqueHandle token(hToken);

        DWORD tokenInfoSize = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenInfoSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return L"";

        std::vector<BYTE> buffer(tokenInfoSize);
        PTOKEN_USER tokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
        if (!GetTokenInformation(hToken, TokenUser, tokenUser, tokenInfoSize, &tokenInfoSize))
            return L"";

        LPWSTR sidString = nullptr;
        if (ConvertSidToStringSidW(tokenUser->User.Sid, &sidString)) {
            UniqueLocalPtr<wchar_t> sidGuard(sidString);
            return std::wstring(sidString);
        }
        return L"";
    }

    /**
     * @brief Checks if the current process is running with elevated (high) integrity level.
     * @return true if the process is elevated (administrator token with high IL), false otherwise.
     *
     * @note This is different from is_admin() which only checks group membership.
     *       A process can be admin but not elevated if UAC is enabled and the user
     *       didn't elevate.
     */
    [[nodiscard]] inline bool is_process_elevated() noexcept {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            return false;
        UniqueHandle token(hToken);

        DWORD size = 0;
        GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &size);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
            return false;

        std::vector<BYTE> buffer(size);
        PTOKEN_MANDATORY_LABEL tml = reinterpret_cast<PTOKEN_MANDATORY_LABEL>(buffer.data());
        if (!GetTokenInformation(hToken, TokenIntegrityLevel, tml, size, &size))
            return false;

        PSID sid = tml->Label.Sid;
        DWORD rid = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
        return rid >= SECURITY_MANDATORY_HIGH_RID;
    }

    // =========================================================================
    // 8. Network adapters, IP addresses, MAC
    // =========================================================================

    /**
     * @brief Information about a physical network adapter.
     */
    struct NetworkAdapter {
        std::wstring name;            ///< Friendly name of the adapter (e.g., "Realtek PCIe GbE")
        std::wstring mac_address;     ///< MAC address in format XX-XX-XX-XX-XX-XX
        unsigned long long speed_mbps;///< Link speed in megabits per second
        bool is_up;                   ///< true if the adapter is operational and connected
    };

    /**
     * @brief Enumerates all non-loopback, up network adapters.
     * @return std::vector<NetworkAdapter> containing each adapter's info.
     *
     * Requires Winsock initialization; uses the internal WinsockGuard.
     */
    [[nodiscard]] inline std::vector<NetworkAdapter> get_network_adapters() {
        detail::WinsockGuard ws;
        if (!ws) return {};

        ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        ULONG size = 0;
        DWORD result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size);
        if (result != ERROR_BUFFER_OVERFLOW) return {};

        std::vector<BYTE> buffer;
        for (int retry = 0; retry < 3; ++retry) {
            buffer.resize(size);
            result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
            if (result == ERROR_SUCCESS) break;
            if (result == ERROR_BUFFER_OVERFLOW) continue;
            return {};
        }
        if (result != ERROR_SUCCESS) return {};

        PIP_ADAPTER_ADDRESSES addr_info = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        std::vector<NetworkAdapter> result_vec;
        for (PIP_ADAPTER_ADDRESSES p = addr_info; p; p = p->Next) {
            if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK || p->OperStatus != IfOperStatusUp)
                continue;

            NetworkAdapter na;
            na.name = p->FriendlyName ? p->FriendlyName : L"";
            na.is_up = (p->OperStatus == IfOperStatusUp);
            na.speed_mbps = p->TransmitLinkSpeed / 1000000;

            if (p->PhysicalAddressLength == 6) {
                wchar_t mac[18] = {};
                _snwprintf_s(mac, _TRUNCATE, L"%02X-%02X-%02X-%02X-%02X-%02X",
                    p->PhysicalAddress[0], p->PhysicalAddress[1], p->PhysicalAddress[2],
                    p->PhysicalAddress[3], p->PhysicalAddress[4], p->PhysicalAddress[5]);
                na.mac_address = mac;
            }
            result_vec.push_back(std::move(na));
        }
        return result_vec;
    }

    /**
     * @brief Returns the first non-loopback IPv4 address of a network adapter.
     * @return IPv4 address as std::wstring (e.g., L"192.168.1.10"), or empty if none found.
     */
    [[nodiscard]] inline std::wstring get_local_ipv4() {
        detail::WinsockGuard ws;
        if (!ws) return L"";

        ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        ULONG size = 0;
        DWORD result = GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size);
        if (result != ERROR_BUFFER_OVERFLOW) return L"";

        std::vector<BYTE> buffer;
        for (int retry = 0; retry < 3; ++retry) {
            buffer.resize(size);
            result = GetAdaptersAddresses(AF_INET, flags, nullptr,
                                          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
            if (result == ERROR_SUCCESS) break;
            if (result == ERROR_BUFFER_OVERFLOW) continue;
            return L"";
        }
        if (result != ERROR_SUCCESS) return L"";

        PIP_ADAPTER_ADDRESSES addr_info = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        for (PIP_ADAPTER_ADDRESSES p = addr_info; p; p = p->Next) {
            if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK || p->OperStatus != IfOperStatusUp)
                continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS u = p->FirstUnicastAddress; u; u = u->Next) {
                if (u->Address.lpSockaddr->sa_family == AF_INET) {
                    SOCKADDR_IN* sa = reinterpret_cast<SOCKADDR_IN*>(u->Address.lpSockaddr);
                    wchar_t ip_str[16];
                    if (InetNtopW(AF_INET, &sa->sin_addr, ip_str, 16)) {
                        std::wstring ip = ip_str;
                        if (ip != L"127.0.0.1" && ip != L"0.0.0.0")
                            return ip;
                    }
                }
            }
        }
        return L"";
    }

    /**
     * @brief Returns the first global (non-link-local, non-loopback) IPv6 address.
     * @return IPv6 address as std::wstring, or empty if none found.
     */
    [[nodiscard]] inline std::wstring get_local_ipv6() {
        detail::WinsockGuard ws;
        if (!ws) return L"";

        ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        ULONG size = 0;
        DWORD result = GetAdaptersAddresses(AF_INET6, flags, nullptr, nullptr, &size);
        if (result != ERROR_BUFFER_OVERFLOW) return L"";

        std::vector<BYTE> buffer;
        for (int retry = 0; retry < 3; ++retry) {
            buffer.resize(size);
            result = GetAdaptersAddresses(AF_INET6, flags, nullptr,
                                          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
            if (result == ERROR_SUCCESS) break;
            if (result == ERROR_BUFFER_OVERFLOW) continue;
            return L"";
        }
        if (result != ERROR_SUCCESS) return L"";

        PIP_ADAPTER_ADDRESSES addr_info = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        for (PIP_ADAPTER_ADDRESSES p = addr_info; p; p = p->Next) {
            if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK || p->OperStatus != IfOperStatusUp)
                continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS u = p->FirstUnicastAddress; u; u = u->Next) {
                if (u->Address.lpSockaddr->sa_family == AF_INET6) {
                    SOCKADDR_IN6* sa = reinterpret_cast<SOCKADDR_IN6*>(u->Address.lpSockaddr);
                    wchar_t ip_str[46];
                    if (InetNtopW(AF_INET6, &sa->sin6_addr, ip_str, 46)) {
                        std::wstring ip = ip_str;
                        if (ip.find(L"fe80:") != 0 && ip != L"::1")
                            return ip;
                    }
                }
            }
        }
        return L"";
    }

    /**
     * @brief Returns the MAC address of the first operational network adapter.
     * @return MAC address as std::wstring in XX-XX-XX-XX-XX-XX format, or empty if none found.
     */
    [[nodiscard]] inline std::wstring get_mac_address() {
        auto adapters = get_network_adapters();
        for (const auto& a : adapters) {
            if (a.is_up && !a.mac_address.empty())
                return a.mac_address;
        }
        return L"";
    }

    // =========================================================================
    // 9. Battery status (laptops only)
    // =========================================================================

    /**
     * @brief Current battery status.
     */
    struct BatteryStatus {
        int percentage;           ///< Battery charge percentage (0-100), -1 if unknown.
        bool is_charging;         ///< true if the battery is currently charging.
        bool is_discharging;      ///< true if the battery is discharging (on battery power).
        bool is_battery_present;  ///< true if a battery is physically present.
        int remaining_minutes;    ///< Estimated remaining time in minutes, -1 if unknown.
    };

    /**
     * @brief Retrieves battery status using GetSystemPowerStatus.
     * @return BatteryStatus structure; if no battery is present, is_battery_present is false.
     */
    [[nodiscard]] inline BatteryStatus get_battery_status() noexcept {
        BatteryStatus status;
        SYSTEM_POWER_STATUS sps;
        if (!GetSystemPowerStatus(&sps))
            return status;

        bool present = (sps.BatteryFlag & 128) == 0;
        status.is_battery_present = present;
        if (!present)
            return status;

        if (sps.BatteryLifePercent != 255)
            status.percentage = static_cast<int>(sps.BatteryLifePercent);

        if (sps.ACLineStatus == 1) {
            status.is_charging = true;
            status.is_discharging = false;
        } else if (sps.ACLineStatus == 0) {
            status.is_charging = false;
            status.is_discharging = true;
        }

        if (sps.BatteryLifeTime != (DWORD)-1)
            status.remaining_minutes = static_cast<int>(sps.BatteryLifeTime / 60);

        return status;
    }

    // =========================================================================
    // 10. Environment variables
    // =========================================================================

    /**
     * @brief Retrieves all environment variables as a vector of name-value pairs.
     * @return std::vector<std::pair<std::wstring, std::wstring>>.
     */
    [[nodiscard]] inline std::vector<std::pair<std::wstring, std::wstring>> get_all_env_vars() noexcept {
        std::vector<std::pair<std::wstring, std::wstring>> result;
        UniqueEnvStrings env_guard(GetEnvironmentStringsW());
        if (!env_guard) return result;

        LPWCH env = env_guard.get();
        for (LPWCH p = env; *p; p += wcslen(p) + 1) {
            std::wstring var(p);
            size_t pos = var.find(L'=');
            if (pos != std::wstring::npos)
                result.emplace_back(var.substr(0, pos), var.substr(pos + 1));
        }
        return result;
    }

    /**
     * @brief Gets the value of a specific environment variable.
     * @param name Name of the variable (e.g., L"PATH").
     * @return Value as std::wstring, or empty if the variable does not exist.
     */
    [[nodiscard]] inline std::wstring get_env_var(const std::wstring& name) noexcept {
        DWORD size = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
        if (size == 0) return L"";
        std::wstring value(size, L'\0');
        DWORD written = GetEnvironmentVariableW(name.c_str(), value.data(), size);
        if (written == 0 || written > size) return L"";
        value.resize(written);
        return value;
    }

    // =========================================================================
    // 11. Windows services
    // =========================================================================

    /**
     * @brief Information about a Windows service.
     */
    struct ServiceInfo {
        std::wstring name;         ///< Internal service name (e.g., "W32Time").
        std::wstring display_name; ///< Display name (e.g., "Windows Time").
        std::wstring status;       ///< Current status: "Running", "Stopped", "Start Pending", etc.
        std::wstring start_type;   ///< Startup type: "Auto", "Manual", "Disabled", "Boot", "System".
    };

    /**
     * @brief Enumerates all Win32 services (both active and inactive).
     * @return std::vector<ServiceInfo> containing all services.
     *
     * Opens the SCM with SC_MANAGER_ENUMERATE_SERVICE, queries using EnumServicesStatusExW,
     * and retrieves startup type by opening each service individually.
     */
    [[nodiscard]] inline std::vector<ServiceInfo> get_services() {
        std::vector<ServiceInfo> result;
        UniqueScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
        if (!scm) return result;

        DWORD bytes_needed = 0, services_returned = 0, resume_handle = 0;
        std::vector<BYTE> buffer;
        EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                              SERVICE_ACTIVE | SERVICE_INACTIVE, nullptr, 0,
                              &bytes_needed, &services_returned, &resume_handle, nullptr);
        if (GetLastError() != ERROR_MORE_DATA) return result;

        buffer.resize(bytes_needed);
        if (!EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                                   SERVICE_ACTIVE | SERVICE_INACTIVE, buffer.data(),
                                   static_cast<DWORD>(buffer.size()), &bytes_needed,
                                   &services_returned, &resume_handle, nullptr)) {
            return result;
        }

        LPENUM_SERVICE_STATUS_PROCESS services = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESS>(buffer.data());
        for (DWORD i = 0; i < services_returned; ++i) {
            ServiceInfo si;
            si.name = services[i].lpServiceName;
            si.display_name = services[i].lpDisplayName;

            switch (services[i].ServiceStatusProcess.dwCurrentState) {
                case SERVICE_RUNNING: si.status = L"Running"; break;
                case SERVICE_STOPPED: si.status = L"Stopped"; break;
                case SERVICE_PAUSED: si.status = L"Paused"; break;
                case SERVICE_START_PENDING: si.status = L"Start Pending"; break;
                case SERVICE_STOP_PENDING: si.status = L"Stop Pending"; break;
                case SERVICE_PAUSE_PENDING: si.status = L"Pause Pending"; break;
                default: si.status = L"Unknown"; break;
            }

            UniqueScHandle svc(OpenServiceW(scm.get(), si.name.c_str(), SERVICE_QUERY_CONFIG));
            if (svc) {
                DWORD config_size = 0;
                QueryServiceConfigW(svc.get(), nullptr, 0, &config_size);
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && config_size > 0) {
                    std::vector<BYTE> config_buf(config_size);
                    LPQUERY_SERVICE_CONFIGW config = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(config_buf.data());
                    if (QueryServiceConfigW(svc.get(), config, config_size, &config_size)) {
                        switch (config->dwStartType) {
                            case SERVICE_AUTO_START: si.start_type = L"Auto"; break;
                            case SERVICE_DEMAND_START: si.start_type = L"Manual"; break;
                            case SERVICE_DISABLED: si.start_type = L"Disabled"; break;
                            case SERVICE_BOOT_START: si.start_type = L"Boot"; break;
                            case SERVICE_SYSTEM_START: si.start_type = L"System"; break;
                            default: si.start_type = L"Unknown"; break;
                        }
                    }
                }
            }
            result.push_back(std::move(si));
        }
        return result;
    }

    // =========================================================================
    // 12. Audio devices (using WinMM)
    // =========================================================================

    /**
     * @brief Returns the number of audio output (playback) devices.
     * @return Count of waveOut devices.
     */
    [[nodiscard]] inline unsigned int get_audio_output_devices_count() noexcept {
        return waveOutGetNumDevs();
    }

    /**
     * @brief Returns the number of audio input (recording) devices.
     * @return Count of waveIn devices.
     */
    [[nodiscard]] inline unsigned int get_audio_input_devices_count() noexcept {
        return waveInGetNumDevs();
    }

    /**
     * @brief Detailed information about an audio device (output or input).
     */
    struct AudioDeviceInfo {
        std::wstring product_name;  ///< Product name (e.g., "Speakers (Realtek Audio)")
        unsigned int device_id;     ///< Zero-based device identifier.
        unsigned short channels;    ///< Number of audio channels supported.
    };

    /**
     * @brief Enumerates all audio output devices.
     * @return std::vector<AudioDeviceInfo>.
     */
    [[nodiscard]] inline std::vector<AudioDeviceInfo> get_audio_output_devices() {
        std::vector<AudioDeviceInfo> result;
        unsigned int count = waveOutGetNumDevs();
        for (unsigned int i = 0; i < count; ++i) {
            WAVEOUTCAPSW caps;
            if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                AudioDeviceInfo info;
                info.product_name = caps.szPname;
                info.device_id = i;
                info.channels = caps.wChannels;
                result.push_back(std::move(info));
            }
        }
        return result;
    }

    /**
     * @brief Enumerates all audio input devices.
     * @return std::vector<AudioDeviceInfo>.
     */
    [[nodiscard]] inline std::vector<AudioDeviceInfo> get_audio_input_devices() {
        std::vector<AudioDeviceInfo> result;
        unsigned int count = waveInGetNumDevs();
        for (unsigned int i = 0; i < count; ++i) {
            WAVEINCAPSW caps;
            if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                AudioDeviceInfo info;
                info.product_name = caps.szPname;
                info.device_id = i;
                info.channels = caps.wChannels;
                result.push_back(std::move(info));
            }
        }
        return result;
    }

    // =========================================================================
    // 13. Process termination
    // =========================================================================

    /**
     * @brief Terminates a process by its PID.
     * @param pid Process ID to terminate.
     * @return true if successful, false otherwise (e.g., access denied or process already exited).
     *
     * @note This forcibly terminates the process; it does not allow clean shutdown.
     *       Use with caution.
     */
    [[nodiscard]] inline bool terminate_process(DWORD pid) noexcept {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!hProcess) return false;
        bool result = (TerminateProcess(hProcess, 1) != 0);
        CloseHandle(hProcess);
        return result;
    }

} // namespace syswin