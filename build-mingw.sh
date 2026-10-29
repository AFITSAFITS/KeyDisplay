#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
cd -- "$(dirname -- "$0")"
cxx="${CXX:-x86_64-w64-mingw32-g++}"
"$cxx" -std=c++17 -Wall -Wextra -Werror -O2 -static -s -mwindows \
  main.cpp -o KeyDisplay.exe -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid
printf '%s\n' 'Built KeyDisplay.exe (Windows x64)'
