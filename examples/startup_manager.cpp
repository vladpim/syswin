#include "syswin.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <windows.h>

void list_startup() {
    HKEY hKey;
    std::wcout << L"\n=== Startup entries (HKCU) ===\n";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        wchar_t name[256];
        BYTE data[1024];
        DWORD nameSize, dataSize, type;
        int count = 0;
        while (true) {
            nameSize = 256;
            dataSize = sizeof(data);
            LONG res = RegEnumValueW(hKey, index, name, &nameSize, nullptr, &type, data, &dataSize);
            if (res == ERROR_NO_MORE_ITEMS) break;
            if (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
                std::wstring command((wchar_t*)data, dataSize / sizeof(wchar_t));
                while (!command.empty() && command.back() == L'\0') command.pop_back();
                std::wcout << L"  [" << count << L"] " << name << L" -> " << command << L'\n';
                ++count;
            }
            ++index;
        }
        RegCloseKey(hKey);
        if (count == 0) std::wcout << L"  (no entries)\n";
    } else {
        std::wcout << L"  (cannot open registry key)\n";
    }
}

int main() {
    std::cout << "========== Startup Manager ==========\n";
    std::cout << "Commands:\n";
    std::cout << "  list                     - show all startup entries\n";
    std::cout << "  add <path> [name]        - add program (name optional)\n";
    std::cout << "  remove <name>            - remove entry by exact name\n";
    std::cout << "  exit                     - quit\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "list") {
            list_startup();
        }
        else if (cmd == "add") {
            std::string path, name;
            if (!(iss >> path)) {
                std::cout << "Usage: add <full_path> [name]\n";
                continue;
            }
            if (!(iss >> name)) {
                size_t pos = path.find_last_of("\\/");
                name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
            }
            std::wstring wpath = syswin::to_wstring(path);
            std::wstring wname = syswin::to_wstring(name);
            if (syswin::add_startup_current_user(wname, wpath)) {
                std::cout << "Added \"" << name << "\" to startup.\n";
            } else {
                std::cout << "Failed to add (invalid path or access denied).\n";
            }
        }
        else if (cmd == "remove") {
            std::string name;
            if (!(iss >> name)) {
                std::cout << "Usage: remove <entry_name>\n";
                continue;
            }
            std::wstring wname = syswin::to_wstring(name);
            if (syswin::remove_startup_current_user(wname)) {
                std::cout << "Removed \"" << name << "\" from startup.\n";
            } else {
                std::cout << "Failed to remove (entry not found or access denied).\n";
            }
        }
        else if (cmd == "exit" || cmd == "q") {
            break;
        }
        else {
            std::cout << "Unknown command. Use 'list', 'add', 'remove', 'exit'.\n";
        }
    }
    return 0;
}