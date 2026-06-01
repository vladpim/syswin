<!-- markdownlint-disable MD033 -->
<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/Windows-Vista%2B-brightgreen.svg" alt="Windows"/>
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="MIT License"/>
  <img src="https://img.shields.io/badge/header--only-yes-orange.svg" alt="Header-only"/>
  <img src="https://img.shields.io/badge/version-1.0 Stable-red.svg" alt="Version"/>
</p>

# 🪟 syswin.hpp – Windows System Information Library

**Single‑header, modern C++ library to query every corner of Windows**  
*No external dependencies, no DLLs – just `#include` and go.*

```cpp
#include "syswin.hpp"
std::string cpu = syswin::get_cpu_name();   // "Intel Core i7-9700K @ 3.60GHz"
