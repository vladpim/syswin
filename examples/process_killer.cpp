#include "syswin.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>

// Очистка экрана консоли (не относится к библиотеке, оставляем)
void clear_screen() {
    system("cls");
}

// Вывод списка процессов в виде таблицы
void display_processes(const std::vector<syswin::Process>& procs) {
    std::cout << "\n=== Running Processes ===\n";
    std::cout << std::left << std::setw(8) << "PID"
              << std::setw(30) << "Name"
              << std::setw(12) << "Memory (MB)" << "\n";
    std::cout << std::string(50, '-') << "\n";
    for (const auto& p : procs) {
        std::cout << std::left << std::setw(8) << p.pid
                  << std::setw(30) << syswin::to_utf8(p.name)
                  << std::setw(12) << p.memory_mb << "\n";
    }
    std::cout << "Total: " << procs.size() << " processes.\n";
}

int main() {
    std::cout << "===== Console Task Manager (syswin library) =====\n";
    std::cout << "Commands:\n";
    std::cout << "  update (u)  - refresh process list\n";
    std::cout << "  kill <PID>  - terminate process by PID\n";
    std::cout << "  exit (q)    - quit\n";
    std::cout << "  clear       - clear screen\n\n";

    std::vector<syswin::Process> processes;
    bool running = true;
    std::string input;

    while (running) {
        if (processes.empty()) {
            processes = syswin::get_running_processes();
            std::sort(processes.begin(), processes.end(),
                      [](const syswin::Process& a, const syswin::Process& b) { return a.pid < b.pid; });
            display_processes(processes);
        }

        std::cout << "\n> ";
        std::getline(std::cin, input);
        if (input.empty()) continue;

        std::stringstream ss(input);
        std::string cmd;
        ss >> cmd;

        if (cmd == "update" || cmd == "u") {
            processes = syswin::get_running_processes();
            std::sort(processes.begin(), processes.end(),
                      [](const syswin::Process& a, const syswin::Process& b) { return a.pid < b.pid; });
            clear_screen();
            std::cout << "===== Console Task Manager (syswin library) =====\n";
            std::cout << "Commands: update (u), kill <PID>, exit (q), clear\n\n";
            display_processes(processes);
        }
        else if (cmd == "kill") {
            DWORD pid;
            if (ss >> pid) {
                if (syswin::terminate_process(pid)) {
                    std::cout << "Process " << pid << " terminated.\n";
                    // Обновляем список после завершения
                    processes = syswin::get_running_processes();
                    std::sort(processes.begin(), processes.end(),
                              [](const syswin::Process& a, const syswin::Process& b) { return a.pid < b.pid; });
                    clear_screen();
                    std::cout << "===== Console Task Manager (syswin library) =====\n";
                    std::cout << "Commands: update (u), kill <PID>, exit (q), clear\n\n";
                    display_processes(processes);
                } else {
                    std::cout << "Failed to terminate process " << pid
                              << " (insufficient rights or process doesn't exist).\n";
                }
            } else {
                std::cout << "Usage: kill <PID>\n";
            }
        }
        else if (cmd == "clear") {
            clear_screen();
            std::cout << "===== Console Task Manager (syswin library) =====\n";
            std::cout << "Commands: update (u), kill <PID>, exit (q), clear\n\n";
            display_processes(processes);
        }
        else if (cmd == "exit" || cmd == "q") {
            running = false;
        }
        else if (cmd == "help") {
            std::cout << "Commands:\n";
            std::cout << "  update (u)  - refresh process list\n";
            std::cout << "  kill <PID>  - terminate process by PID\n";
            std::cout << "  exit (q)    - quit\n";
            std::cout << "  clear       - clear screen\n";
        }
        else {
            std::cout << "Unknown command. Type 'help' for commands.\n";
        }
    }

    std::cout << "Goodbye!\n";
    return 0;
}