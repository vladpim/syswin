<!-- markdownlint-disable MD033 -->
<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/Windows-Vista%2B-brightgreen.svg" alt="Windows"/>
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="MIT License"/>
  <img src="https://img.shields.io/badge/header--only-yes-orange.svg" alt="Header-only"/>
  <img src="https://img.shields.io/badge/version-1.0 Stable-red.svg" alt="Version"/>
</p>

# syswin.hpp – Windows System Information Library

**Single‑header, modern C++ library to query every corner of Windows**  
*No external dependencies, no DLLs – just `#include` and go.*

```cpp
#include "syswin.hpp"
std::string cpu = syswin::get_cpu_name();   //supposedly "Intel Core i7-9700K @ 3.60GHz"
```

## Category	What you can get

Hardware	CPU name, physical/logical cores, GPU name, total RAM, disk info (size, free space, file system)

Live telemetry	CPU usage (delta), RAM used/free (GB), current process memory

Processes	List all running processes (PID, name, path, memory), kill a process by PID

Startup	Read/write/delete HKCU/HKLM Run entries with environment variable expansion

OS & System	Windows version (real), build number, uptime, computer name, architecture, system directories

Administrator	Check admin rights, check elevation level, relaunch as admin (UAC)

Installed SW	Enumerate 32/64‑bit installed applications from registry (name, version, publisher, uninstall)

Current user	Username, domain, SID

Network	List adapters (name, MAC, speed, status), get local IPv4/IPv6, get MAC address

Battery	Percentage, charging state, remaining minutes (laptops)

Windows Services	List all services with status (Running/Stopped) and startup type (Auto/Manual/Disabled)

Audio devices	Count and details of output/input devices (WinMM)

Environment	Get all variables or a specific one

## Installation
Just copy syswin.hpp into your project and include it.
That’s it – no build steps, no extra libraries. All necessary .lib files are automatically linked via #pragma comment.

```bash
git clone https://github.com/yourname/syswin.git
cd your_project
cp syswin/syswin.hpp .
```

